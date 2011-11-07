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

#ifndef WEBSOCKET_SERVER_HPP
#define WEBSOCKET_SERVER_HPP

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

#include "rng/blank_rng.hpp"

#include "http/parser.hpp"
#include "logger/logger.hpp"

using boost::asio ::ip::tcp;

namespace websocketpp {


// TODO: potential policies:
// - http parser
template <typename rng_policy = blank_rng, template <class> class logger_type = log::logger>
class server : public boost::enable_shared_from_this< server<rng_policy> > {
public:
	typedef rng_policy rng_t;
	
	typedef server<rng_policy> endpoint_type;
	typedef session<endpoint_type> session_type;
	typedef connection_handler<session_type> connection_handler_type;
	
	typedef boost::shared_ptr<endpoint_type> ptr;
	typedef boost::shared_ptr<session_type> session_ptr;
	typedef boost::shared_ptr<connection_handler_type> connection_handler_ptr;
	
	server<rng_policy>(boost::asio::io_service& io_service, 
		   const tcp::endpoint& endpoint) 
	: m_max_message_size(DEFAULT_MAX_MESSAGE_SIZE),
	  m_io_service(io_service), 
	  m_acceptor(io_service, endpoint), 
	  m_desc("websocketpp::server") 
	{
		m_desc.add_options()
		  ("help", "produce help message")
		  ("host,h",po::value<std::vector<std::string> >()->multitoken()->composing(), "hostnames to listen on")
		  ("port,p",po::value<int>(), "port to listen on")
		;
	}
	
	void set_default_connection_handler(connection_handler_ptr c) {
		m_def_con_handler = c;
	}
	
	// creates a new session object and connects the next websocket
	// connection to it.
	void start_accept() {
		if (m_def_con_handler == connection_handler_ptr()) {
			throw server_error("start_accept called before a connection handler was set");
		}
		
		// TODO: sanity check whether the session buffer size bound could be reduced
		session_ptr new_session(
			new session_type(
				endpoint_type::shared_from_this(),
				m_io_service,
				m_def_con_handler,
				m_max_message_size*2
			)
		);
		
		m_acceptor.async_accept(
			new_session->socket(),
			boost::bind(
				&endpoint_type::handle_accept,
				endpoint_type::shared_from_this(),
				new_session,
				boost::asio::placeholders::error
			)
		);
	}
	
	template <template <class> class T>
	connection_handler_ptr make_handler() {
		return boost::shared_ptr< T<session_type> >(new T<session_type>());
	}
	
	// INTERFACE FOR LOCAL APPLICATIONS
	void set_max_message_size(uint64_t val) {
		if (val > frame::limits::PAYLOAD_SIZE_JUMBO) {
			// TODO: Figure out what the ideal error behavior for this method.
			// Options:
			//   Throw exception
			//   Log error and set value to maximum allowed
			//   Log error and leave value at whatever it was before
			elog().at(log::elevel::WARN) << "Invalid maximum message size: " 
			                          << val << log::endl;
			//throw server_error(err.str());
		}
		m_max_message_size = val;
	}
	
	void parse_command_line(int ac, char* av[]) {
		po::store(po::parse_command_line(ac,av, m_desc),m_vm);
		po::notify(m_vm);
		
		if (m_vm.count("help") ) {
			std::cout << m_desc << std::endl;
		}
		
		//m_vm["host"].as<std::string>();
		
		// TODO: template "as" weirdness 
		// const std::vector< std::string > &foo = m_vm["host"].as< std::vector<std::string> >();
		
		const std::vector< std::string > &foo = m_vm["host"].template as< std::vector<std::string> >();
		
		for (int i = 0; i < foo.size(); i++) {
			std::cout << foo[i] << std::endl;
		}
		
		//std::cout << m_vm["host"].as< std::vector<std::string> >() << std::endl;
	}
	
	// INTERFACE FOR SESSIONS
	
	static const bool is_server = true;
	
	rng_policy& get_rng() {
		return m_rng;
	}
	
	// checks a handshake for validity. Returns true if valid and throws a 
	// handshake_error otherwise
	bool validate_handshake(const http::parser::request& handshake) {
		std::stringstream err;
		std::string h;
		
		if (handshake.method() != "GET") {
			err << "Websocket handshake has invalid method: " 
			    << handshake.method();
			
			throw(handshake_error(err.str(),http::status_code::BAD_REQUEST));
		}
		
		// TODO: allow versions greater than 1.1
		if (handshake.version() != "HTTP/1.1") {
			err << "Websocket handshake has invalid HTTP version: " 
			<< handshake.method();
			
			throw(handshake_error(err.str(),http::status_code::BAD_REQUEST));
		}
		
		// verify the presence of required headers
		h = handshake.header("Host");
		if (h == "") {
			throw(handshake_error("Required Host header is missing",http::status_code::BAD_REQUEST));
		} else if (!this->validate_host(h)) {
			err << "Host " << h << " is not one of this server's names.";
			throw(handshake_error(err.str(),http::status_code::BAD_REQUEST));
		}
		
		h = handshake.header("Upgrade");
		if (h == "") {
			throw(handshake_error("Required Upgrade header is missing",http::status_code::BAD_REQUEST));
		} else if (!boost::ifind_first(h,"websocket")) {
			err << "Upgrade header \"" << h << "\", does not contain required token \"websocket\"";
			throw(handshake_error(err.str(),http::status_code::BAD_REQUEST));
		}
		
		h = handshake.header("Connection");
		if (h == "") {
			throw(handshake_error("Required Connection header is missing",http::status_code::BAD_REQUEST));
		} else if (!boost::ifind_first(h,"upgrade")) {
			err << "Connection header, \"" << h 
			<< "\", does not contain required token \"upgrade\"";
			throw(handshake_error(err.str(),http::status_code::BAD_REQUEST));
		}
		
		if (handshake.header("Sec-WebSocket-Key") == "") {
			throw(handshake_error("Required Sec-WebSocket-Key header is missing",http::status_code::BAD_REQUEST));
		}
		
		h = handshake.header("Sec-WebSocket-Version");
		if (h == "") {
			// TODO: if we want to support draft 00 this line should set version to 0
			// rather than bail
			throw(handshake_error("Required Sec-WebSocket-Version header is missing",http::status_code::BAD_REQUEST));
		} else {
			int version = atoi(h.c_str());
			
			if (version != 7 && version != 8 && version != 13) {
				err << "This server doesn't support WebSocket protocol version "
				<< version;
				throw(handshake_error(err.str(),http::status_code::BAD_REQUEST));
			}
		}
		
		return true;
	}
	
	// Confirms that the port in the host string matches the port we are listening
	// on. End user application is responsible for checking the /host/ part.
	bool validate_host(std::string host) {
		// find colon.
		// if no colon assume default port
		
		// if port == port
		// return true
		// else
		// return false
		
		// TODO: just check the port. Otherwise user is responsible for checking this
		
		return true;
	}
	
	// Check if message size is within server's acceptable parameters
	bool validate_message_size(uint64_t val) {
		if (val > m_max_message_size) {
			return false;
		}
		return true;
	}
	
	logger_type<log::alevel::value>& alog() {
		return m_alog;
	}
	
	logger_type<log::elevel::value>& elog() {
		return m_elog;
	}
	
private:
	// if no errors starts the session's read loop and returns to the
	// start_accept phase.
	void handle_accept(session_ptr session,const boost::system::error_code& error) 
	{
		if (!error) {
			session->read_request();
		} else {
			std::stringstream err;
			err << "Error accepting socket connection: " << error;
			
			elog().at(log::elevel::ERROR) << err.str() << log::endl;
			throw server_error(err.str());
		}
		
		this->start_accept();
	}
	
private:
	logger_type<log::alevel::value>	m_alog;
	logger_type<log::elevel::value> m_elog;
	
	std::vector<session_ptr>	m_sessions;
	
	uint64_t					m_max_message_size;
	boost::asio::io_service&	m_io_service;
	tcp::acceptor				m_acceptor;
	connection_handler_ptr		m_def_con_handler;
	
	rng_policy					m_rng;
	
	po::options_description		m_desc;
	po::variables_map			m_vm;
};

}

#endif // WEBSOCKET_SERVER_HPP
