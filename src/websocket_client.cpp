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
#include "websocket_client.hpp"

#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <iostream>

using websocketpp::client;
using boost::asio::ip::tcp;

client::client(boost::asio::io_service& io_service,
               websocketpp::connection_handler_ptr defc)
	: m_elog_level(LOG_OFF),
	  m_alog_level(ALOG_OFF),
	  m_state(CLIENT_STATE_NULL),
	  m_max_message_size(DEFAULT_MAX_MESSAGE_SIZE),
	  m_io_service(io_service),
	  m_resolver(io_service),
	  m_def_con_handler(defc) {}

void client::init() {
	// TODO: sanity check whether the session buffer size bound could be reduced
	m_client_session = client_session_ptr(
		new client_session(
	    	shared_from_this(),
			m_io_service,
			m_def_con_handler,
			m_max_message_size*2
		)
	);
	m_state = CLIENT_STATE_INITIALIZED;
}

void client::connect(const std::string& uri) {
	if (m_state != CLIENT_STATE_INITIALIZED) {
		throw client_error("connect can only be called after init and before a connection has been established");
	}
	
	m_client_session->set_uri(uri);
	
	std::stringstream port;
	port << m_client_session->get_port();


	tcp::resolver::query query(m_client_session->get_host(),
	                           port.str());
	tcp::resolver::iterator iterator = m_resolver.resolve(query);
	
	boost::asio::async_connect(m_client_session->socket(),
	                           iterator,boost::bind(&client::handle_connect,
							   this,
							   boost::asio::placeholders::error)); 
	m_state = CLIENT_STATE_CONNECTING;
}


void client::add_subprotocol(const std::string& p) {
	if (m_state != CLIENT_STATE_INITIALIZED) {
		throw client_error("add_protocol can only be called after init and before connect");
	}
	m_client_session->add_subprotocol(p);
}

void client::set_header(const std::string& key,const std::string& val) {
	if (m_state != CLIENT_STATE_INITIALIZED) {
		throw client_error("set_header can only be called after init and before connect");
	}
	m_client_session->set_header(key,val);
}

void client::set_origin(const std::string& val) {
	if (m_state != CLIENT_STATE_INITIALIZED) {
		throw client_error("set_origin can only be called after init and before connect");
	}
	m_client_session->set_origin(val);
}


void client::set_max_message_size(uint64_t val) {
	if (val > frame::PAYLOAD_64BIT_LIMIT) {
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

bool client::test_elog_level(uint16_t level) {
	return (level >= m_elog_level);
}
void client::set_elog_level(uint16_t level) {
	std::stringstream msg;
	msg << "Error logging level changing from " 
	    << m_elog_level << " to " << level;
	log(msg.str(),LOG_INFO);

	m_elog_level = level;
}
bool client::test_alog_level(uint16_t level) {
	return ((level & m_alog_level) != 0);
}
void client::set_alog_level(uint16_t level) {
	if (test_alog_level(level)) {
		return;
	}
	std::stringstream msg;
	msg << "Access logging level " << level << " being set"; 
	access_log(msg.str(),ALOG_INFO);

	m_alog_level |= level;
}
void client::unset_alog_level(uint16_t level) {
	if (!test_alog_level(level)) {
		return;
	}
	std::stringstream msg;
	msg << "Access logging level " << level << " being unset"; 
	access_log(msg.str(),ALOG_INFO);

	m_alog_level &= ~level;
}

bool client::validate_message_size(uint64_t val) {
	if (val > m_max_message_size) {
		return false;
	}
	return true;
}

void client::log(std::string msg,uint16_t level) {
	if (!test_elog_level(level)) {
		return;
	}
	std::cerr << "[Error Log] "
	          << boost::posix_time::to_iso_extended_string(
			         boost::posix_time::second_clock::local_time())
              << " " << msg << std::endl;
}
void client::access_log(std::string msg,uint16_t level) {
	if (!test_alog_level(level)) {
		return;
	}
	std::cout << "[Access Log] " 
	          << boost::posix_time::to_iso_extended_string(
			         boost::posix_time::second_clock::local_time())
              << " " << msg << std::endl;
}

void client::handle_connect(const boost::system::error_code& error) {
	if (!error) {
		std::stringstream err;
		err << "Successful Connection ";
		log(err.str(),LOG_ERROR);
		
		std::cout << boost::posix_time::to_iso_extended_string(boost::posix_time::microsec_clock::local_time()) << " TCP established" << std::endl;
		
		m_state = CLIENT_STATE_CONNECTED;
		m_client_session->on_connect();
	} else {
		std::stringstream err;
		err << "An error occurred while establishing a connection: " << error;
		
		log(err.str(),LOG_ERROR);
		throw client_error(err.str());
	}
}
