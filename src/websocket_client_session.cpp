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
#include "websocket_client_session.hpp"

#include "websocket_frame.hpp"
#include "utf8_validator/utf8_validator.hpp"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/random.hpp>
#include <boost/random/random_device.hpp>


#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

using websocketpp::client_session;

client_session::client_session (websocketpp::client_ptr c,
                                boost::asio::io_service& io_service,
                                websocketpp::connection_handler_ptr defc,
								uint64_t buf_size)
	: session(io_service,defc,buf_size),m_client(c) {}

void client_session::on_connect() {
	// TODO: section 4.1: Figure out if we have another connection to this 
	// host/port pending.
	write_handshake();
}

void client_session::set_uri(const std::string& uri) {
	if (!m_uri.parse(uri)) {
		throw client_error("Invalid WebSocket URI");
	}

	if (m_uri.secure) {
		throw client_error("wss / secure connections are not supported at this time");
	}
	
	m_resource = m_uri.resource;
	
	std::stringstream l;
	
	l << "parsed websocket url: secure: " << m_uri.secure << " host: " << m_uri.host
	  << " port (final): " << m_uri.port << " resource " << m_uri.resource;
	
	log(l.str(),LOG_DEBUG);
}

bool client_session::get_secure() const {
	return m_uri.secure;
}

std::string client_session::get_host() const{
	return m_uri.host;
}

uint16_t client_session::get_port() const {
	return m_uri.port;
}

void client_session::set_header(const std::string &key,const std::string &val) {
	// TODO: prevent use of reserved headers
	m_client_headers[key] = val;
}

void client_session::set_origin(const std::string& val) {
	// TODO: input validation
	m_client_origin = val;
}

void client_session::add_subprotocol(const std::string &val) {
	// TODO: input validation
	m_client_subprotocols.push_back(val);
}

void client_session::add_extension(const std::string& val) {
	// TODO: input validation
	m_client_extensions.push_back(val);
}

void client_session::read_handshake() {
	boost::asio::async_read_until(
		m_socket,
		m_buf,
			"\r\n\r\n",
		boost::bind(
			&session::handle_read_handshake,
			shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred
		)
	);
}

void client_session::handle_read_handshake(const boost::system::error_code& e,
	std::size_t bytes_transferred) {
	
	if (e) {
		log_error("Error reading server handshake",e);
		drop_tcp();
		return;
	}
	
	// parse server handshake
	std::istream response_stream(&m_buf);
	std::string header;
	std::string::size_type end;
	
	// get status line
	std::getline(response_stream, header);
	if (header[header.size()-1] == '\r') {
		header.erase(header.end()-1);
		m_server_http_request = header;
		m_raw_server_handshake += header+"\n";
	}
	
	// get headers
	while (std::getline(response_stream, header) && header != "\r") {
		if (header[header.size()-1] != '\r') {
			continue; // ignore malformed header lines?
		} else {
			header.erase(header.end()-1);
		}
		
		end = header.find(": ",0);
		
		if (end != std::string::npos) {			
			std::string h = header.substr(0,end);
			if (get_server_header(h) == "") {
				m_server_headers[h] = header.substr(end+2);
			} else {
				m_server_headers[h] += ", " + header.substr(end+2);
			}
		}
		
		m_raw_server_handshake += header+"\n";
	}
	
	// temporary debugging
	if (m_buf.size() > 0) {
		std::stringstream foo;
		foo << "bytes left over: " << m_buf.size();
		access_log(foo.str(), ALOG_HANDSHAKE);
	}
	
	m_client->access_log(m_raw_server_handshake,ALOG_HANDSHAKE);
	
	// handshake error checking
	try {
		std::stringstream err;
		std::string h;
		
		// TODO: allow versions greater than 1.1
		if (m_server_http_request.substr(0,9) != "HTTP/1.1 ") {
			err << "Websocket handshake has invalid HTTP version: "
				<< m_server_http_request.substr(0,9);
			
			throw(handshake_error(err.str(),400));
		}
		
		// check the HTTP version
		if (m_server_http_request.substr(9,3) != "101") {
			err << "Websocket handshake ended with status "
			    << m_server_http_request.substr(9);
			
			// TODO: check version header for other supported versions.
			
			throw(handshake_error(err.str(),400));
		}
		
		// verify the presence of required headers
		h = get_server_header("Upgrade");
		if (h == "") {
			throw(handshake_error("Required Upgrade header is missing",400));
		} else if (!boost::iequals(h,"websocket")) {
			err << "Upgrade header was \"" << h << "\" instead of \"websocket\"";
			throw(handshake_error(err.str(),400));
		}
		
		h = get_server_header("Connection");
		if (h == "") {
			throw(handshake_error("Required Connection header is missing",400));
		} else if (!boost::ifind_first(h,"upgrade")) {
			err << "Connection header, \"" << h 
				<< "\", does not contain required token \"upgrade\"";
			throw(handshake_error(err.str(),400));
		}
			
		if (get_server_header("Sec-WebSocket-Accept") == "") {
			throw(handshake_error("Required Sec-WebSocket-Key header is missing",400));
		} else {
			// TODO: make a helper function for this.
			std::string server_key = m_client_key;
			server_key += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
			
			SHA1		sha;
			uint32_t	message_digest[5];
			
			sha.Reset();
			sha << server_key.c_str();
	
			if (!sha.Result(message_digest)) {
				m_client->log("Error computing handshake sha1 hash.",LOG_ERROR);
				// TODO: close behavior
				return;
			}
	
			// convert sha1 hash bytes to network byte order because this sha1
			//  library works on ints rather than bytes
			for (int i = 0; i < 5; i++) {
				message_digest[i] = htonl(message_digest[i]);
			}
	
			server_key = base64_encode(
					reinterpret_cast<const unsigned char*>(message_digest),20);
			if (server_key != get_server_header("Sec-WebSocket-Accept")) {
				m_client->log("Server key does not match",LOG_ERROR);
				// TODO: close behavior
				return;
			}
		}
	} catch (const handshake_error& e) {
		std::stringstream err;
		err << "Caught handshake exception: " << e.what();
		
		m_client->access_log(e.what(),ALOG_HANDSHAKE);
		m_client->log(err.str(),LOG_ERROR);
		
		// TODO: close behavior
		return;
	}
	
	log_open_result();

	m_state = STATE_OPEN;

	if (m_local_interface) {
		m_local_interface->on_open(shared_from_this());
	}
	
	reset_message();
	read_frame();
}

void client_session::write_handshake() {
	// generate client handshake.	
	std::string client_handshake;
	
	client_handshake += "GET "+m_resource+" HTTP/1.1\r\n";
	
	set_header("Upgrade","websocket");
	set_header("Connection","Upgrade");
	set_header("Sec-WebSocket-Version","13");
	
	std::stringstream h;
	
	h << m_uri.host << ":" << m_uri.port;
	
	set_header("Host",h.str());

	if (m_client_origin != "") {
		set_header("Origin",m_client_origin);
	}
	
	// TODO: generate proper key
	m_client_key = "XO4pxrIMLnK1CEVQP9untQ==";
	
	int32_t raw_key[4];
	
	boost::random::random_device rng;
	boost::random::variate_generator<boost::random::random_device&, boost::random::uniform_int_distribution<> > gen(rng, boost::random::uniform_int_distribution<>(INT32_MIN,INT32_MAX));
	
	for (int i = 0; i < 4; i++) {
		raw_key[i] = gen();
	}
	
	m_client_key = base64_encode(reinterpret_cast<unsigned char const*>(raw_key), 16);
	
	m_client->access_log("Client key chosen: "+m_client_key, ALOG_HANDSHAKE);
	
	set_header("Sec-WebSocket-Key",m_client_key);
	
	

	set_header("User Agent","WebSocket++/2011-09-25");

	header_list::iterator it;
	for (it = m_client_headers.begin(); it != m_client_headers.end(); it++) {
		client_handshake += it->first + ": " + it->second + "\r\n";
	}
	
	client_handshake += "\r\n";
	
	m_raw_client_handshake = client_handshake;

	// start async write to handle_write_handshake
	boost::asio::async_write(
		m_socket,
		boost::asio::buffer(m_raw_client_handshake),
		boost::bind(
			&session::handle_write_handshake,
			shared_from_this(),
			boost::asio::placeholders::error
		)
	);
}

void client_session::handle_write_handshake(const boost::system::error_code& error) {
	if (error) {
		log_error("Error writing handshake",error);
		drop_tcp();
		return;
	}
	
	read_handshake();
}

void client_session::log(const std::string& msg, uint16_t level) const {
	m_client->log(msg,level);
}

void client_session::access_log(const std::string& msg, uint16_t level) const {
	m_client->access_log(msg,level);
}
