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
#include <boost/enable_shared_from_this.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <set>

#include "websocketpp.hpp"
#include "websocket_session.hpp"
#include "websocket_connection_handler.hpp"

#include <boost/date_time/posix_time/posix_time.hpp>

#include "rng/boost_rng.hpp"

#include "http/parser.hpp"

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

template <typename rng_policy = boost_rng>
class client : public boost::enable_shared_from_this<client<rng_policy> > {
public:
	typedef rng_policy rng_t;
	
	typedef client<rng_policy> endpoint_type;
	typedef session<endpoint_type> session_type;
	typedef connection_handler<session_type> connection_handler_type;
	
	typedef boost::shared_ptr<endpoint_type> ptr;
	typedef boost::shared_ptr<session_type> session_ptr;
	typedef boost::shared_ptr<connection_handler_type> connection_handler_ptr;
		
	static const uint16_t CLIENT_STATE_NULL = 0;
	static const uint16_t CLIENT_STATE_INITIALIZED = 1;
	static const uint16_t CLIENT_STATE_CONNECTING = 2;
	static const uint16_t CLIENT_STATE_CONNECTED = 3;

	client<rng_policy>(boost::asio::io_service& io_service,
		   connection_handler_ptr defc)
	: m_elog_level(LOG_OFF),
	  m_alog_level(ALOG_OFF),
	  m_state(CLIENT_STATE_NULL),
	  m_max_message_size(DEFAULT_MAX_MESSAGE_SIZE),
	  m_io_service(io_service),
	  m_resolver(io_service),
	  m_def_con_handler(defc) {}

	// INTERFACE FOR LOCAL APPLICATIONS
	
	// initializes the session. Methods that affect the opening handshake
	// such as add_protocol and set_header must be called after init and
	// before connect.
	void init() {
		// TODO: sanity check whether the session buffer size bound could be reduced
		m_client_session = session_ptr(
			new session_type(
				endpoint_type::shared_from_this(),
				m_io_service,
				m_def_con_handler,
				m_max_message_size*2
			)
		);
		m_state = CLIENT_STATE_INITIALIZED;
	}

	// starts the connection process. Should be called before 
	// io_service.run(), connection process will not start until run() has
	// been called.
	void connect(const std::string& u) {
		if (m_state != CLIENT_STATE_INITIALIZED) {
			throw client_error("connect can only be called after init and before a connection has been established");
		}
		
		ws_uri uri;
		
		if (!uri.parse(u)) {
			throw client_error("Invalid WebSocket URI");
		}
		
		if (uri.secure) {
			throw client_error("wss / secure connections are not supported at this time");
		}
		
		m_client_session->set_uri(uri);
		
		std::stringstream port;
		port << uri.port;
		
		tcp::resolver::query query(uri.host,port.str());
		tcp::resolver::iterator iterator = m_resolver.resolve(query);
		
		boost::asio::async_connect(
			m_client_session->socket(),
			iterator,
			boost::bind(
				&endpoint_type::handle_connect,
				endpoint_type::shared_from_this(),
				boost::asio::placeholders::error
			)
		); 
		m_state = CLIENT_STATE_CONNECTING;
	}
	
	// Adds a protocol to the opening handshake.
	// Must be called before connect
	void add_subprotocol(const std::string& p) {
		if (m_state != CLIENT_STATE_INITIALIZED) {
			throw client_error("add_protocol can only be called after init and before connect");
		}
		m_client_session->add_subprotocol(p);
	}
	
	// Sets the value of the given HTTP header to be sent during the 
	// opening handshake. Must be called before connect
	void set_header(const std::string& key,const std::string& val) {
		if (m_state != CLIENT_STATE_INITIALIZED) {
			throw client_error("set_header can only be called after init and before connect");
		}
		m_client_session->set_request_header(key,val);
	}
	
	void set_origin(const std::string& val) {
		if (m_state != CLIENT_STATE_INITIALIZED) {
			throw client_error("set_origin can only be called after init and before connect");
		}
		m_client_session->set_origin(val);
	}

	void set_max_message_size(uint64_t val) {
		if (val > frame::limits::PAYLOAD_SIZE_JUMBO) {
			std::stringstream err;
			err << "Invalid maximum message size: " << val;
			
			// TODO: Figure out what the ideal error behavior for this method.
			// Options:
			//   Throw exception
			//   Log error and set value to maximum allowed
			//   Log error and leave value at whatever it was before
			log(err.str(),LOG_WARN);
			//throw client_error(err.str());
		}
		m_max_message_size = val;
	}
	
	// Test methods determine if a message of the given level should be 
	// written. elog shows all values above the level set. alog shows only
	// the values explicitly set.
	bool test_elog_level(uint16_t level) {
		return (level >= m_elog_level);
	}
	void set_elog_level(uint16_t level) {
		std::stringstream msg;
		msg << "Error logging level changing from " 
	    << m_elog_level << " to " << level;
		log(msg.str(),LOG_INFO);
		
		m_elog_level = level;
	}
	
	bool test_alog_level(uint16_t level) {
		return ((level & m_alog_level) != 0);
	}
	void set_alog_level(uint16_t level) {
		if (test_alog_level(level)) {
			return;
		}
		std::stringstream msg;
		msg << "Access logging level " << level << " being set"; 
		access_log(msg.str(),ALOG_INFO);
		
		m_alog_level |= level;
	}
	void unset_alog_level(uint16_t level) {
		if (!test_alog_level(level)) {
			return;
		}
		std::stringstream msg;
		msg << "Access logging level " << level << " being unset"; 
		access_log(msg.str(),ALOG_INFO);
		
		m_alog_level &= ~level;
	}

	// INTERFACE FOR SESSIONS

	// Check if message size is within server's acceptable parameters
	bool validate_message_size(uint64_t val) {
		if (val > m_max_message_size) {
			return false;
		}
		return true;
	}
	
	// write to the server's logs
	void log(std::string msg,uint16_t level = LOG_ERROR) {
		if (!test_elog_level(level)) {
			return;
		}
		std::cerr << "[Error Log] "
		          << boost::posix_time::to_iso_extended_string(
		                 boost::posix_time::second_clock::local_time())
		          << " " << msg << std::endl;
	}
	void access_log(std::string msg,uint16_t level) {
		if (!test_alog_level(level)) {
			return;
		}
		std::cout << "[Access Log] " 
		          << boost::posix_time::to_iso_extended_string(
		                 boost::posix_time::second_clock::local_time())
		          << " " << msg << std::endl;
	}
private:
	// if no errors starts the session's read loop and returns to the
	// start_accept phase.
	void handle_connect(const boost::system::error_code& error) {
		if (!error) {
			std::stringstream err;
			err << "Successful Connection ";
			log(err.str(),LOG_ERROR);
			
			m_state = CLIENT_STATE_CONNECTED;
			m_client_session->on_connect();
		} else {
			std::stringstream err;
			err << "An error occurred while establishing a connection: " << error;
			
			log(err.str(),LOG_ERROR);
			throw client_error(err.str());
		}
	}
	
private:
	uint16_t					m_elog_level;
	uint16_t					m_alog_level;
	
	uint16_t					m_state;

	std::set<std::string>		m_hosts;
	uint64_t					m_max_message_size;
	boost::asio::io_service&	m_io_service;
	tcp::resolver				m_resolver;
	session_ptr					m_client_session;
	connection_handler_ptr		m_def_con_handler;
};

}

#endif // WEBSOCKET_CLIENT_HPP
