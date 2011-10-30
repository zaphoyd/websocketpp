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

#include "policy/rng/blank_rng.hpp"

using boost::asio::ip::tcp;

namespace websocketpp {



template <typename rng_policy = blank_rng>
class server : public boost::enable_shared_from_this< server<rng_policy> > {
public:
	typedef rng_policy rng_t;
	
	typedef server<rng_policy> server_type;
	typedef session<server_type> session_type;
	typedef connection_handler<session_type> connection_handler_type;
	
	typedef boost::shared_ptr<server_type> ptr;
	typedef boost::shared_ptr<session_type> session_ptr;
	typedef boost::shared_ptr<connection_handler_type> connection_handler_ptr;
	
	server<rng_policy>(boost::asio::io_service& io_service, 
		   const tcp::endpoint& endpoint) 
	: m_elog_level(LOG_ALL),
	  m_alog_level(ALOG_ALL),
	  m_max_message_size(DEFAULT_MAX_MESSAGE_SIZE),
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
				server_type::shared_from_this(),
				m_io_service,
				m_def_con_handler,
				m_max_message_size*2
			)
		);
		
		m_acceptor.async_accept(
			new_session->socket(),
			boost::bind(
				&server_type::handle_accept,
				server_type::shared_from_this(),
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
			std::stringstream err;
			err << "Invalid maximum message size: " << val;
			
			// TODO: Figure out what the ideal error behavior for this method.
			// Options:
			//   Throw exception
			//   Log error and set value to maximum allowed
			//   Log error and leave value at whatever it was before
			log(err.str(),LOG_WARN);
			//throw server_error(err.str());
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
	void handle_accept(session_ptr session,const boost::system::error_code& error) 
	{
		if (!error) {
			session->on_connect();
		} else {
			std::stringstream err;
			err << "Error accepting socket connection: " << error;
			
			log(err.str(),LOG_ERROR);
			throw server_error(err.str());
		}
		
		this->start_accept();
	}
	
private:
	uint16_t					m_elog_level;
	uint16_t					m_alog_level;
	
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
