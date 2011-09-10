/*
 * Copyright (c) 2011, Peter Thorson. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the WebSocket++ Project nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL PETER THORSON BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#ifndef WEBSOCKET_SESSION_HPP
#define WEBSOCKET_SESSION_HPP

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <arpa/inet.h>

#include <algorithm>
#include <exception>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace websocketpp {
	class session;
	typedef boost::shared_ptr<session> session_ptr;
	
	class handshake_error;
}

#include "websocket_server.hpp"
#include "websocket_frame.hpp"
#include "websocket_connection_handler.hpp"

#include "base64/base64.h"
#include "sha1/sha1.h"

using boost::asio::ip::tcp;

namespace websocketpp {

class session : public boost::enable_shared_from_this<session> {
public:
	enum ws_status {
		CONNECTING,
		OPEN,
		CLOSING,
		CLOSED
	};
	
	typedef enum ws_status status_code;
	
	friend class handshake_error;
	
	session (server_ptr s,
			 boost::asio::io_service& io_service,
			 connection_handler_ptr defc);
	
	tcp::socket& socket();
	
	/*** SERVER INTERFACE ***/
	
	// This function is called to begin the session loop. This method and all
	// that come after it are called as a result of an async event completing.
	// if any method in this chain returns before adding a new async event the
	// session will end.
	void start();
	
	// sets the internal connection handler of this connection to new_con.
	// This is useful if you want to switch handler objects during a connection
	// Example: a generic lobby handler could validate the handshake negotiate a
	// sub protocol to talk to and then pass the connection off to a handler for
	// that sub protocol.
	void set_handler(connection_handler_ptr new_con);
	
	
	/*** HANDSHAKE INTERFACE ***/
	
	// gets the value of a header or the empty string if not present.
	std::string get_header(const std::string &key) const;
	// adds an arbitrary header to the server handshake HTTP response.
	void add_header(const std::string &key,const std::string &value);
	
	std::string get_request() const;
	std::string get_origin() const;
	
	// sets the subprotocol being used. This will result in the appropriate 
	// Sec-WebSocket-Protocol header being sent back to the client. The value
	// here must have been present in the client's opening handshake.
	void set_subprotocol(const std::string &protocol);
	

	//int get_version();

	//void add_extension();
	
	/*** SESSION INTERFACE ***/
	
	// send basic frame types
	void send(const std::string &msg); // text
	void send(const std::vector<unsigned char> &data); // binary
	void ping(const std::string &msg);
	void pong(const std::string &msg);
	
	void disconnect(uint16_t status,const std::string &reason);
private:	
	// handle_read_handshake reads the HTTP headers of the initial websocket
	// handshake, parses out the request and headers, and does error checking
	// TODO: Generalize a lot of the hard coded things in this method.
	void handle_read_handshake(const boost::system::error_code& e,
		std::size_t bytes_transferred);
	
	// write_handshake calculates the server portion of the handshake and 
	// sends it back.
	// TODO: Generalize this to include things like protocols, cookies, etc
	void write_handshake();
	// handle_write_handshake checks for errors writing the server handshake,
	// officially declares a connection open, notifies the local interface,
	// and starts the frame reading loop.
	void handle_write_handshake(const boost::system::error_code& error);
	
	// construct and write an HTTP error in the case the handshake goes poorly
	void write_http_error(int http_code,const std::string &http_err_str);
	void handle_write_http_error(const boost::system::error_code& error);
	
	// start async read for a websocket frame (2 bytes) to handle_frame_header
	void read_frame();
	
	// reads frame header and devices if it needs to read more header or go
	// straight to the payload.
	void handle_frame_header(const boost::system::error_code& error);
	
	// process extra headers and start payload read
	void handle_extended_frame_header(const boost::system::error_code& error);
	
	// initiate payload read
	void read_payload();
	
	// now the frame object should be complete. Process and send it on then 
	// reset for new frame
	void handle_read_payload (const boost::system::error_code& error);
	
	// checks for errors writing frames
	void handle_write_frame (const boost::system::error_code& error);
	
	// helper functions for processing each opcode
	void process_ping();
	void process_pong();
	void process_text();
	void process_binary();
	void process_continuation();
	void process_close();
	
	// deliver message if we have a local interface attached
	void deliver_message();
	
	// copies the current read frame payload into the session so that the read
	// frame can be cleared for the next read. This is done when fragmented
	// messages are recieved.
	void extract_payload();
	
	// write m_write_frame out to the socket.
	void write_frame();
	
	// reset session for a new message
	void reset_message();
	
	// prints a diagnostic message and disconnects the local interface
	void handle_error(std::string msg,const boost::system::error_code& error);
private:
	// Immutable state about the current connection from the handshake
	std::string 						m_request;
	std::map<std::string,std::string>	m_headers;
	unsigned int						m_version;
	std::string							m_subprotocol;
	
	// Mutable connection state;
	status_code				m_status;
	
	// Connection Resources
	server_ptr				m_server;
	tcp::socket 			m_socket;
	connection_handler_ptr	m_local_interface;
	
	// Buffers
	boost::asio::streambuf m_buf;
	
	// unorganized
	std::string m_handshake;
	frame m_read_frame;
	frame m_write_frame;
	std::vector<unsigned char> m_current_message;
	bool m_error;
	bool m_fragmented;
	frame::opcode m_current_opcode;
};

// Exception classes

class handshake_error : public std::exception {
public:	
	handshake_error(const std::string& msg,
					int http_error,
					const std::string& http_msg = "")
		: m_msg(msg),m_http_error_code(http_error),m_http_error_msg(http_msg) {}
	~handshake_error() throw() {}
	
	virtual const char* what() const throw() {
		return m_msg.c_str();
	}
	
	void write(session_ptr s) const {
		s->write_http_error(m_http_error_code,m_http_error_msg);
	}
	
private:
	std::string m_msg;
	int			m_http_error_code;
	std::string m_http_error_msg;
};

}

#endif // WEBSOCKET_SESSION_HPP



// better debug printing system
// set acceptible origin and host headers
// case sensitive header values? e.g. websocket


// double check bugs in autobahn (sending wrong localhost:9000 header) not 
// checking masking in the 9.x tests
