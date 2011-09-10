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

#include "websocket_server.hpp"

#include <boost/bind.hpp>

#include <iostream>

using websocketpp::server;

server::server(boost::asio::io_service& io_service, 
			   const tcp::endpoint& endpoint,
			   connection_handler_ptr defc)
	: m_max_message_size(DEFAULT_MAX_MESSAGE_SIZE),
	  m_io_service(io_service), 
	  m_acceptor(io_service, endpoint), 
	  m_def_con_handler(defc) {}

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
		throw server_error(err.str());
	}
	m_max_message_size = val;
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

void server::error_log(std::string msg) {
	std::cerr << "[Error Log] " << msg << std::endl;
}
void server::access_log(std::string msg) {
	std::cout << "[Access Log] " << msg << std::endl;
}

void server::start_accept() {
	session_ptr new_ws(new session(shared_from_this(),
								   m_io_service,
								   m_def_con_handler));
	
	m_acceptor.async_accept(
		new_ws->socket(),
		boost::bind(
			&server::handle_accept,
			this,
			new_ws,
			boost::asio::placeholders::error
		)
	);
}

void server::handle_accept(session_ptr session,
	const boost::system::error_code& error) {
	
	if (!error) {
		session->start();
	} else {
		std::stringstream err;
		err << "Error accepting socket connection: " << error;
		
		error_log(err.str());
		throw server_error(err.str());
	}
	
	this->start_accept();
}