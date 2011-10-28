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

#ifndef WEBSOCKET_CLIENT_HPP
#define WEBSOCKET_CLIENT_HPP

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>

#include <set>

namespace websocketpp {
	class client;
	typedef boost::shared_ptr<client> client_ptr;
}

#include "websocketpp.hpp"
#include "websocket_client_session.hpp"
#include "websocket_connection_handler.hpp"

using boost::asio::ip::tcp;

namespace websocketpp {

class client_error : public std::exception {
public:	
	client_error(const std::string& msg)
		: m_msg(msg) {}
	~client_error() throw() {}
	
	virtual const char* what() const throw() {
		return m_msg.c_str();
	}
private:
	std::string m_msg;
};

class client : public boost::enable_shared_from_this<client> {
	public:
		static const uint16_t CLIENT_STATE_NULL = 0;
		static const uint16_t CLIENT_STATE_INITIALIZED = 1;
		static const uint16_t CLIENT_STATE_CONNECTING = 2;
		static const uint16_t CLIENT_STATE_CONNECTED = 3;

		client(boost::asio::io_service& io_service,
			   connection_handler_ptr defc);

		// INTERFACE FOR LOCAL APPLICATIONS
		
		// initializes the session. Methods that affect the opening handshake
		// such as add_protocol and set_header must be called after init and
		// before connect.
		void init();

		// starts the connection process. Should be called before 
		// io_service.run(), connection process will not start until run() has
		// been called.
		void connect(const std::string& url);
		
		// Adds a protocol to the opening handshake.
		// Must be called before connect
		void add_subprotocol(const std::string& p);
		
		// Sets the value of the given HTTP header to be sent during the 
		// opening handshake. Must be called before connect
		void set_header(const std::string& key,const std::string& val);
		
		void set_origin(const std::string& val);

		void set_max_message_size(uint64_t val);
		
		// Test methods determine if a message of the given level should be 
		// written. elog shows all values above the level set. alog shows only
		// the values explicitly set.
		bool test_elog_level(uint16_t level);
		void set_elog_level(uint16_t level);
		
		bool test_alog_level(uint16_t level);
		void set_alog_level(uint16_t level);
		void unset_alog_level(uint16_t level);

		// INTERFACE FOR SESSIONS

		// Check if message size is within server's acceptable parameters
		bool validate_message_size(uint64_t val);
		
		// write to the server's logs
		void log(std::string msg,uint16_t level = LOG_ERROR);
		void access_log(std::string msg,uint16_t level);
	private:
		// if no errors starts the session's read loop and returns to the
		// start_accept phase.
		void handle_connect(const boost::system::error_code& error);
		
	private:
		uint16_t					m_elog_level;
		uint16_t					m_alog_level;
		
		uint16_t					m_state;

		std::set<std::string>		m_hosts;
		uint64_t					m_max_message_size;
		boost::asio::io_service&	m_io_service;
		tcp::resolver				m_resolver;
		client_session_ptr			m_client_session;
		connection_handler_ptr		m_def_con_handler;
};

}

#endif // WEBSOCKET_CLIENT_HPP
