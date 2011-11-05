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

#include "websocketpp.hpp"
#include "websocket_server.hpp"

#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>

#include <iostream>

using websocketpp::server;

server::server(boost::asio::io_service& io_service, 
			   const tcp::endpoint& endpoint,
			   websocketpp::connection_handler_ptr defc)
	: m_elog_level(LOG_ERROR),
	  m_alog_level(ALOG_CONTROL),
	  m_max_message_size(DEFAULT_MAX_MESSAGE_SIZE),
	  m_io_service(io_service), 
	  m_acceptor(io_service, endpoint), 
	  m_def_con_handler(defc),
	  m_desc("websocketpp::server") {
	  m_desc.add_options()
		  ("help", "produce help message")
		  ("host,h",po::value<std::vector<std::string> >()->multitoken()->composing(), "hostnames to listen on")
		  ("port,p",po::value<int>(), "port to listen on")
	;
	  
}

void server::parse_command_line(int ac, char* av[]) {
	po::store(po::parse_command_line(ac,av, m_desc),m_vm);
	po::notify(m_vm);
	
	if (m_vm.count("help") ) {
		std::cout << m_desc << std::endl;
	}
	
	//m_vm["host"].as<std::string>();
	
	const std::vector< std::string > &foo = m_vm["host"].as< std::vector<std::string> >();
	
	for (int i = 0; i < foo.size(); i++) {
		std::cout << foo[i] << std::endl;
	}
	
	//std::cout << m_vm["host"].as< std::vector<std::string> >() << std::endl;
}

void server::add_host(std::string host) {
	m_hosts.insert(host);
}

void server::remove_host(std::string host) {
	m_hosts.erase(host);
}


void server::set_max_message_size(uint64_t val) {
	if (val > frame::PAYLOAD_64BIT_LIMIT) {
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

bool server::test_elog_level(uint16_t level) {
	return (level >= m_elog_level);
}
void server::set_elog_level(uint16_t level) {
	std::stringstream msg;
	msg << "Error logging level changing from " 
	    << m_elog_level << " to " << level;
	log(msg.str(),LOG_INFO);

	m_elog_level = level;
}
bool server::test_alog_level(uint16_t level) {
	return ((level & m_alog_level) != 0);
}
void server::set_alog_level(uint16_t level) {
	if (test_alog_level(level)) {
		return;
	}
	std::stringstream msg;
	msg << "Access logging level " << level << " being set"; 
	access_log(msg.str(),ALOG_INFO);

	m_alog_level |= level;
}
void server::unset_alog_level(uint16_t level) {
	if (!test_alog_level(level)) {
		return;
	}
	std::stringstream msg;
	msg << "Access logging level " << level << " being unset"; 
	access_log(msg.str(),ALOG_INFO);

	m_alog_level &= ~level;
}

bool server::validate_host(std::string host) {
	if (m_hosts.find(host) == m_hosts.end()) {
		return false;
	}
	return true;
}

bool server::validate_message_size(uint64_t val) {
	if (val > m_max_message_size) {
		return false;
	}
	return true;
}

void server::log(std::string msg,uint16_t level) {
	if (!test_elog_level(level)) {
		return;
	}
	std::cerr << "[Error Log] "
	          << boost::posix_time::to_iso_extended_string(
			         boost::posix_time::second_clock::local_time())
              << " " << msg << std::endl;
}
void server::access_log(std::string msg,uint16_t level) {
	if (!test_alog_level(level)) {
		return;
	}
	std::cout << "[Access Log] " 
	          << boost::posix_time::to_iso_extended_string(
			         boost::posix_time::second_clock::local_time())
              << " " << msg << std::endl;
}

void server::start_accept() {
	// TODO: sanity check whether the session buffer size bound could be reduced
	server_session_ptr new_session(new server_session(shared_from_this(),
	                                                  m_io_service,
	                                                  m_def_con_handler,
													  m_max_message_size*2));
	
	m_acceptor.async_accept(
		new_session->socket(),
		boost::bind(
			&server::handle_accept,
			this,
			new_session,
			boost::asio::placeholders::error
		)
	);
}

void server::handle_accept(websocketpp::server_session_ptr session,
	const boost::system::error_code& error) {
	
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
