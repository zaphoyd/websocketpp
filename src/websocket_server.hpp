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

#include "interfaces/session.hpp"

#include "websocket_session.hpp"
#include "websocket_connection_handler.hpp"

#include <boost/date_time/posix_time/posix_time.hpp>

#include "rng/blank_rng.hpp"

#include "http/parser.hpp"
#include "logger/logger.hpp"

using boost::asio ::ip::tcp;
using websocketpp::session::server_handler_ptr;

namespace websocketpp {
namespace server {

template <typename server_policy>
class session : public websocketpp::session::server, boost::enable_shared_from_this< session<server_policy> >  {
public:
	typedef server_policy server_type;
	typedef session<server_policy> session_type;
	
	typedef boost::shared_ptr<server_type> server_ptr;
	
	session(server_ptr s,
			boost::asio::io_service& io_service,
			server_handler_ptr handler)
	: m_server(s),
	  m_io_service(io_service),
	  m_socket(io_service),
	  m_timer(io_service,boost::posix_time::seconds(0)),
	  m_buf(/* TODO: needs a max here */),
	  m_handler(handler) {}
	
	tcp::socket& get_socket() {
		return m_socket;
	}
	
	void read_request() {
		// start reading HTTP header and attempt to determine if the incoming 
		// connection is a websocket connection. If it is determine the version
		// and generate a session processor for that version. If it is not a 
		// websocket connection either drop or pass to the default HTTP pass 
		// through handler.
		
		m_timer.expires_from_now(boost::posix_time::seconds(5 /* TODO */));
		
		m_timer.async_wait(
		    boost::bind(
		        &session_type::fail_on_expire,
		        session_type::shared_from_this(),
		        boost::asio::placeholders::error
		    )
		);
		
		boost::asio::async_read_until(
		    m_socket,
		    m_buf,
		    "\r\n\r\n",
		    boost::bind(
		        &session_type::handle_read_request,
		        session_type::shared_from_this(),
		        boost::asio::placeholders::error,
		        boost::asio::placeholders::bytes_transferred
		    )
		);
	}
	
	void handle_read_request(const boost::system::error_code& e,
							 std::size_t bytes_transferred) {
		if (e) {
			log_error("Error reading HTTP request",e);
			drop_tcp();
			return;
		}
		
		try {
			std::istream request(&m_buf);
			
			// TODO: use a more generic consume api where we just call read_some
			// and have the handshake consume and validate as we go.
			// 
			// For now, because it simplifies things we will use the parse_header
			// member function which requires the complete header to be passed in
			// initially. ASIO can guarantee us this.
			//
			// 
			//m_remote_handshake.consume(response_stream);
			if (!m_request.parse_complete(request)) {
				// not a valid HTTP request/response
				throw handshake_error("Recieved invalid HTTP Request",http::status_code::BAD_REQUEST);
			}
			
			// Log the raw handshake.
			m_server->alog().at(log::alevel::DEBUG_HANDSHAKE) << m_request.raw() << log::endl;
			
			// Determine what sort of connection this is:
			int m_version = -1;
			
			if (boost::ifind_first(m_request.header("Upgrade","websocket"))) {
				if (handshake.header("Sec-WebSocket-Version") == "") {
					m_version = 0;
				} else {
					m_version = atoi(h.c_str());
					if (m_version == 0) {
						throw(handshake_error("Unable to determine connection version",http::status_code::BAD_REQUEST));
					}
				}
			}
			
			if (m_version == -1) {
				// Probably a plain HTTP request
				// TODO: forward to an http handler?
			} else {
				// websocket connection
				// create a processor based on version.
				if (m_version == 0) {
					// create hybi 00 processor
					
					// grab hybi00 token first
					char foo[9];
					foo[8] = 0;
					
					request.get(foo,9);
					
					if (request.gcount() != 8) {
						throw handshake_error("Missing Key3",http::status_code::BAD_REQUEST);
					}
					m_request.set_header("Sec-WebSocket-Key3",std::string(foo));
					
					m_processor = protocol::processor_ptr(new protocol::hybi_00_processor());
				} else if (m_version == 7 || m_version == 8 || m_version == 13) {
					// create hybi 17 processor
					m_processor = protocol::processor_ptr(new protocol::hybi_17_processor());
				} else {
					// TODO: respond with unknown version message per spec
				}
				
				// ask new protocol whether this set of headers is valid
				m_processor->validate_handshake(m_request);
				
				// ask local application to confirm that it wants to accept
				m_handler->validate(boost::static_pointer_cast<websocketpp::session::server>(session_type::shared_from_this()));
				
				m_response.set_status(http::status_code::SWITCHING_PROTOCOLS);
			}
			
		} catch (const handshake_error& e) {
			m_server->alog().at(log::alevel::DEBUG_HANDSHAKE) << e.what() << log::endl;
			
			m_server->elog().at(log::elevel::ERROR) 
			<< "Caught handshake exception: " << e.what() << log::endl;
			
			m_response.set_status(e.m_http_error_code,e.m_http_error_msg);
		}
		
		write_response();
	}
	
	// write the response to the client's request.
	void write_response() {
		std::string response;
		
		m_response.set_version("HTTP/1.1");
		
		if (m_response.status_code() == http::status_code::SWITCHING_PROTOCOLS) {
			// websocket response
			m_processor->handshake_response(m_request,m_response);
			
			if (m_subprotocol != "") {
				m_response.replace_header("Sec-WebSocket-Protocol",m_subprotocol);
			}
			
			// TODO: return negotiated extensions
		} else {
			// HTTP response
		}
		
		m_response.replace_header("Server","WebSocket++/2011-10-31");
		
		std::string raw = m_response.raw();
		
		if (m_version == 0) {
			raw += digest;
		}
		
		m_server->alog().at(log::alevel::DEBUG_HANDSHAKE) << raw << log::endl;
		
		// start async write to handle_write_handshake
		boost::asio::async_write(
		    m_socket,
		    boost::asio::buffer(raw),
		    boost::bind(
		        &session_type::handle_write_response,
		        session_type::shared_from_this(),
		        boost::asio::placeholders::error
		    )
		);
	}
	
	void handle_write_response(const boost::system::error_code& error) {
		if (error) {
			log_error("Error writing handshake response",error);
			drop_tcp();
			return;
		}
		
		log_open_result();
		
		if (m_response.status_code() != http::status_code::SWITCHING_PROTOCOLS) {
			m_server->elog().at(log::elevel::ERROR) 
			<< "Handshake ended with HTTP error: " 
			<< m_response.status_code() << " " << m_response.status_msg() 
			<< log::endl;
			
			drop_tcp();
			// TODO: tell client that connection failed?
			// use on_fail?
			return;
		}
		
		m_state = state::OPEN;
		
		// stop the handshake timer
		m_timer.cancel();
		
		m_handler->on_open(boost::static_pointer_cast<websocketpp::session::server>(session_type::shared_from_this()));
		
		// TODO: start read message loop.
	}
	
	void fail_on_expire(const boost::system::error_code& error) {
		if (error) {
			if (error != boost::asio::error::operation_aborted) {
				m_server->elog().at(log::elevel::DEVEL) 
				<< "fail_on_expire timer ended in unknown error" << log::endl;
				//drop_tcp(true);
			}
			return;
		}
		m_server->elog().at(log::elevel::DEVEL) 
		<< "fail_on_expire timer expired" << log::endl;
		drop_tcp(true);
	}
	
private:
	server_ptr					m_server;
	
	boost::asio::io_service&	m_io_service;
	tcp::socket					m_socket;
	boost::asio::deadline_timer	m_timer;
	boost::asio::streambuf		m_buf;
	
	server_handler_ptr			m_handler;
	protocol::processor_ptr		m_processor;
				
	http::parser::request		m_request;
	http::parser::response		m_response;
};


// TODO: potential policies:
// - http parser
template <template <class> class logger_type = log::logger>
class server : public boost::enable_shared_from_this< server<logger_type> > {
public:
	typedef server<logger_type> endpoint_type;
	typedef websocketpp::server::session<endpoint_type> session_type;
	
	typedef boost::shared_ptr<endpoint_type> ptr;
	//typedef websocketpp::session::server_ptr session_ptr;
	typedef boost::shared_ptr<session_type> session_ptr;
	
	server<logger_type>(uint16_t port, server_handler_ptr handler) 
	: m_endpoint(tcp::v6(),port),
	  m_acceptor(m_io_service,m_endpoint),
	  m_handler(handler),
	  m_max_message_size(DEFAULT_MAX_MESSAGE_SIZE),
	  m_desc("websocketpp::server") 
	{
		m_desc.add_options()
		  ("help", "produce help message")
		  ("host,h",po::value<std::vector<std::string> >()->multitoken()->composing(), "hostnames to listen on")
		  ("port,p",po::value<int>(), "port to listen on")
		;
	}
	
	// creates a new session object and connects the next websocket
	// connection to it.
	void start_accept() {
		// TODO: sanity check whether the session buffer size bound could be reduced
		session_ptr new_session(
			new session_type(
				endpoint_type::shared_from_this(),
				m_io_service,
				m_handler
			)
		);
		
		m_acceptor.async_accept(
			new_session->get_socket(),
			boost::bind(
				&endpoint_type::handle_accept,
				endpoint_type::shared_from_this(),
				new_session,
				boost::asio::placeholders::error
			)
		);
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
		
		if (handshake.header("Sec-WebSocket-Key") == "" && handshake.header("Sec-WebSocket-Key1") == "" && handshake.header("Sec-WebSocket-Key2") == "") {
			throw(handshake_error("Required Sec-WebSocket-Key header is missing",http::status_code::BAD_REQUEST));
		}
		
		h = handshake.header("Sec-WebSocket-Version");
		if (h == "") {
			// TODO: if we want to support draft 00 this line should set version to 0
			// rather than bail
			//throw(handshake_error("Required Sec-WebSocket-Version header is missing",http::status_code::BAD_REQUEST));
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
			
			// TODO: add session to local session vector
		} else {
			std::stringstream err;
			err << "Error accepting socket connection: " << error;
			
			elog().at(log::elevel::ERROR) << err.str() << log::endl;
			throw server_error(err.str());
		}
		
		this->start_accept();
	}
	
private:
	boost::asio::io_service	m_io_service;
	tcp::endpoint			m_endpoint;
	tcp::acceptor			m_acceptor;
	
	server_handler_ptr		m_handler;
	
	logger_type<log::alevel::value>	m_alog;
	logger_type<log::elevel::value> m_elog;
	
	std::vector<session_ptr>	m_sessions;
	
	uint64_t					m_max_message_size;	
	
	po::options_description		m_desc;
	po::variables_map			m_vm;
};

}
}

#endif // WEBSOCKET_SERVER_HPP
