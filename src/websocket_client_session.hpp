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

#ifndef WEBSOCKET_CLIENT_SESSION_HPP
#define WEBSOCKET_CLIENT_SESSION_HPP

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
	class client_session;
	typedef boost::shared_ptr<client_session> client_session_ptr;
}

#include "websocket_session.hpp"
#include "websocket_client.hpp"

using boost::asio::ip::tcp;

namespace websocketpp {

class client_session : public session {
public:
	client_session (client_ptr c,
			 boost::asio::io_service& io_service,
			 connection_handler_ptr defc,
			 uint64_t buf_size);

	/*** CLIENT INTERFACE ***/
	
	// This function is called when a tcp connection has been established and 
	// the connection is ready to start the opening handshake.
	void on_connect();
	
	/*** HANDSHAKE INTERFACE ***/
	
	void set_uri(const std::string& url);

	bool get_secure() const;
	std::string get_host() const;
	uint16_t get_port() const;

	// Set an HTTP header for the outgoing client handshake.
	void set_header(const std::string& key,const std::string& val);

	// adds a subprotocol. This will result in the appropriate 
	// Sec-WebSocket-Protocol header being sent with the opening connection.
	// Values will be sent in the order they were added. Servers interpret this
	// order as the preferred order.
	void add_subprotocol(const std::string &val);
	
	// Sets the origin value that will be sent to the server
	void set_origin(const std::string &val);
	
	// Adds an extension to the extension list. Extensions are sent in the 
	// order added
	void add_extension(const std::string& val);
	
	/*** SESSION INTERFACE ***/
	// see session
	bool is_server() const {return false;}

	void log(const std::string& msg, uint16_t level) const;
	void access_log(const std::string& msg, uint16_t level) const;
protected:
	// Opening handshake processors and callbacks.
	virtual void write_handshake();
	virtual void handle_write_handshake(const boost::system::error_code& e);
	virtual void read_handshake();
	virtual void handle_read_handshake(const boost::system::error_code& e,
	                                   std::size_t bytes_transferred);
	
private:
	
protected:
	ws_uri		m_uri;
	// url parts
	bool		m_secure;
	std::string	m_host;
	uint16_t	m_port;

	// handshake stuff
	std::string	m_client_key;

	// connection resources
	client_ptr m_client;
private:
	
};

}

#endif // WEBSOCKET_CLIENT_SESSION_HPP
