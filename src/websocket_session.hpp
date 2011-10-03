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

#if defined(WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

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

#include "websocketpp.hpp"
#include "websocket_frame.hpp"
#include "websocket_connection_handler.hpp"

#include "base64/base64.h"
#include "sha1/sha1.h"

using boost::asio::ip::tcp;

namespace websocketpp {

typedef std::map<std::string,std::string> header_list;

class session : public boost::enable_shared_from_this<session> {
public:
	friend class handshake_error;

	enum ws_status {
		CONNECTING,
		OPEN,
		CLOSING,
		CLOSED
	};
	
	typedef enum ws_status status_code;
	
	static const uint16_t CLOSE_STATUS_NORMAL = 1000;
	static const uint16_t CLOSE_STATUS_GOING_AWAY = 1001;
	static const uint16_t CLOSE_STATUS_PROTOCOL_ERROR = 1002;
	static const uint16_t CLOSE_STATUS_UNSUPPORTED_DATA = 1003;
	static const uint16_t CLOSE_STATUS_NO_STATUS = 1005;
	static const uint16_t CLOSE_STATUS_ABNORMAL_CLOSE = 1006;
	static const uint16_t CLOSE_STATUS_INVALID_PAYLOAD = 1007;
	static const uint16_t CLOSE_STATUS_POLICY_VIOLATION = 1008;
	static const uint16_t CLOSE_STATUS_MESSAGE_TOO_BIG = 1009;
	static const uint16_t CLOSE_STATUS_EXTENSION_REQUIRE = 1010;

	session (boost::asio::io_service& io_service,
			 connection_handler_ptr defc);
	
	tcp::socket& socket();
	boost::asio::io_service& io_service();
	
	/*** SERVER INTERFACE ***/
	
	// This function is called to begin the session loop. This method and all
	// that come after it are called as a result of an async event completing.
	// if any method in this chain returns before adding a new async event the
	// session will end.
	virtual void on_connect() = 0;
	
	// sets the internal connection handler of this connection to new_con.
	// This is useful if you want to switch handler objects during a connection
	// Example: a generic lobby handler could validate the handshake negotiate a
	// sub protocol to talk to and then pass the connection off to a handler for
	// that sub protocol.
	void set_handler(connection_handler_ptr new_con);
	
	
	/*** HANDSHAKE INTERFACE ***/
	// Set session connection information (avaliable only before/during the
	// opening handshake)
	
	// Get session status (valid once the connection is open)
	
	// returns the subprotocol that was negotiated during the opening handshake
	// or the empty string if no subprotocol was requested.
	const std::string& get_subprotocol() const;
	const std::string& get_resource() const;
	const std::string& get_origin() const;
	std::string get_client_header(const std::string& key) const;
	std::string get_server_header(const std::string& key) const;
	const std::vector<std::string>& get_extensions() const;
	unsigned int get_version() const;
	
	/*** SESSION INTERFACE ***/
	
	// send basic frame types
	void send(const std::string &msg); // text
	void send(const std::vector<unsigned char> &data); // binary
	void ping(const std::string &msg);
	void pong(const std::string &msg);
	
	// initiate a connection close
	void close(uint16_t status,const std::string &reason);
	void disconnect(uint16_t status,const std::string& reason); // temp

	virtual bool is_server() const = 0;

	// Opening handshake processors and callbacks. These need to be defined in
	// derived classes.
	virtual void handle_write_handshake(const boost::system::error_code& e) = 0;
	virtual void handle_read_handshake(const boost::system::error_code& e,
	                                   std::size_t bytes_transferred) = 0;
protected:
	virtual void write_handshake() = 0;
	virtual void read_handshake() = 0;
	
	// start async read for a websocket frame (2 bytes) to handle_frame_header
	void read_frame();
	void handle_frame_header(const boost::system::error_code& error);
	void handle_extended_frame_header(const boost::system::error_code& error);
	void read_payload();
	void handle_read_payload (const boost::system::error_code& error);
	
	// write m_write_frame out to the socket.
	void write_frame();
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
	
	// reset session for a new message
	void reset_message();
	
	// logging
	virtual void log(const std::string& msg, uint16_t level) const = 0;
	virtual void access_log(const std::string& msg, uint16_t level) const = 0;
	
	void log_close_result();
	void log_open_result();

	// prints a diagnostic message and disconnects the local interface
	void handle_error(std::string msg,const boost::system::error_code& error);
private:
	std::string get_header(const std::string& key,
	                       const header_list& list) const;

protected:
	// Immutable state about the current connection from the handshake
	// Client handshake
	std::string					m_raw_client_handshake;
	std::string					m_client_http_request;
	std::string					m_resource;
	std::string					m_client_origin;
	header_list					m_client_headers;
	std::vector<std::string>	m_client_subprotocols;
	std::vector<std::string>	m_client_extensions;
	unsigned int				m_version;

	// Server handshake
	std::string					m_raw_server_handshake;
	std::string					m_server_http_request;
	header_list					m_server_headers;
	std::string					m_server_subprotocol;
	std::vector<std::string>	m_server_extensions;
	uint16_t					m_server_http_code;
	std::string					m_server_http_string;

	// Mutable connection state;
	status_code				m_status;
	uint16_t				m_close_code;
	std::string				m_close_message;

	// Close state
	uint16_t	m_local_close_code;
	std::string	m_local_close_msg;
	uint16_t	m_remote_close_code;
	std::string	m_remote_close_msg;
	bool		m_was_clean;
	bool		m_closed_by_me;
	bool		m_dropped_by_me;

	// Connection Resources
	tcp::socket 				m_socket;
	boost::asio::io_service&	m_io_service;
	connection_handler_ptr		m_local_interface;
	
	// Buffers
	boost::asio::streambuf m_buf;
	
	// current message state
	uint32_t					m_utf8_state;
	uint32_t					m_utf8_codepoint;
	std::vector<unsigned char>	m_current_message;
	bool 						m_fragmented;
	frame::opcode 				m_current_opcode;
	
	// current frame state
	frame m_read_frame;

	// unorganized
	frame m_write_frame;
	bool m_error;
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
