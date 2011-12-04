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
#include "websocket_server_session.hpp"

#include "websocket_frame.hpp"
#include "utf8_validator/utf8_validator.hpp"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string.hpp>


#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

using websocketpp::server_session;

server_session::server_session(websocketpp::server_ptr s,
                               boost::asio::io_service& io_service,
                               websocketpp::connection_handler_ptr defc,
							   uint64_t buf_size)
	: session(io_service,defc,buf_size),m_server(s) {}

void server_session::on_connect() {
	read_handshake();
}


void server_session::set_header(const std::string &key,const std::string &val) {
	// TODO: prevent use of reserved headers;
	m_server_headers[key] = val;
}

void server_session::select_subprotocol(const std::string& val) {
	std::vector<std::string>::iterator it;

	it = std::find(m_client_subprotocols.begin(),
	               m_client_subprotocols.end(),
				   val);
	
	if (val != "" && it == m_client_subprotocols.end()) {
		throw server_error("Attempted to choose a subprotocol not proposed by the client");
	}

	m_server_subprotocol = val;
}

void server_session::select_extension(const std::string& val) {
	if (val == "") {
		return;
	}

	std::vector<std::string>::iterator it;

	it = std::find(m_client_extensions.begin(),
	               m_client_extensions.end(),
				   val);

	if (it == m_client_extensions.end()) {
		throw server_error("Attempted to choose an extension not proposed by the client");
	}

	m_server_extensions.push_back(val);
}

void server_session::read_handshake() {
	m_timer.expires_from_now(boost::posix_time::seconds(5));
	
	m_timer.async_wait(
		boost::bind(
			&session::handle_handshake_expired,
			shared_from_this(),
			boost::asio::placeholders::error
		)
	);
	
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

void server_session::handle_read_handshake(const boost::system::error_code& e,
	                                       std::size_t bytes_transferred) {
	std::ostringstream line;
	line << &m_buf;
	m_raw_client_handshake += line.str();
	
	access_log(m_raw_client_handshake,ALOG_HANDSHAKE);
	
	std::vector<std::string> tokens;
	std::string::size_type start = 0;
	std::string::size_type end;
	
	// Get request and parse headers
	end = m_raw_client_handshake.find("\r\n",start);
	
	while(end != std::string::npos) {
		tokens.push_back(m_raw_client_handshake.substr(start, end - start));
		
		start = end + 2;
		
		end = m_raw_client_handshake.find("\r\n",start);
	}

	for (size_t i = 0; i < tokens.size(); i++) {
		if (i == 0) {
			m_client_http_request = tokens[i];
		}
		
		end = tokens[i].find(": ",0);
		
		if (end != std::string::npos) {
			std::string h = tokens[i].substr(0,end);

			if (get_client_header(h) == "") {
				m_client_headers[h] = tokens[i].substr(end+2);
			} else {
				m_client_headers[h] += ", " + tokens[i].substr(end+2);
			}
		}
	}
	
	// handshake error checking
	try {
		std::stringstream err;
		std::string h;
		
		// check the method
		if (m_client_http_request.substr(0,4) != "GET ") {
			err << "Websocket handshake has invalid method: "
				<< m_client_http_request.substr(0,4);
			
			throw(handshake_error(err.str(),400));
		}
		
		// check the HTTP version
		// TODO: allow versions greater than 1.1
		end = m_client_http_request.find(" HTTP/1.1",4);
		if (end == std::string::npos) {
			err << "Websocket handshake has invalid HTTP version";
			throw(handshake_error(err.str(),400));
		}
		
		m_resource = m_client_http_request.substr(4,end-4);
		
		// verify the presence of required headers
		h = get_client_header("Host");
		if (h == "") {
			throw(handshake_error("Required Host header is missing",400));
		} else if (!m_server->validate_host(h)) {
			err << "Host " << h << " is not one of this server's names.";
			throw(handshake_error(err.str(),400));
		}
		
		h = get_client_header("Upgrade");
		if (h == "") {
			throw(handshake_error("Required Upgrade header is missing",400));
		} else if (!boost::iequals(h,"websocket")) {
			err << "Upgrade header was " << h << " instead of \"websocket\"";
			throw(handshake_error(err.str(),400));
		}
		
		h = get_client_header("Connection");
		if (h == "") {
			throw(handshake_error("Required Connection header is missing",400));
		} else if (!boost::ifind_first(h,"upgrade")) {
			err << "Connection header, \"" << h 
				<< "\", does not contain required token \"upgrade\"";
			throw(handshake_error(err.str(),400));
		}
		
		if (get_client_header("Sec-WebSocket-Key") == "") {
			throw(handshake_error("Required Sec-WebSocket-Key header is missing",400));
		}
		
		h = get_client_header("Sec-WebSocket-Version");
		if (h == "") {
			throw(handshake_error("Required Sec-WebSocket-Version header is missing",400));
		} else {
			m_version = atoi(h.c_str());
			
			if (m_version != 7 && m_version != 8 && m_version != 13) {
				err << "This server doesn't support WebSocket protocol version "
					<< m_version;
				throw(handshake_error(err.str(),400));
			}
		}
		
		if (m_version < 13) {
			h = get_client_header("Sec-WebSocket-Origin");
		} else {
			h = get_client_header("Origin");
		}
		
		if (h != "") {
			m_client_origin = h;
		}

		// TODO: extract subprotocols
		// TODO: extract extensions

		// optional headers (delegated to the local interface)
		if (m_local_interface) {
			m_local_interface->validate(shared_from_this());
		}
		
		m_server_http_code = 101;
		m_server_http_string = "Switching Protocols";
	} catch (const handshake_error& e) {
		std::stringstream err;
		err << "Caught handshake exception: " << e.what();
		
		access_log(e.what(),ALOG_HANDSHAKE);
		log(err.str(),LOG_ERROR);
		
		m_server_http_code = e.m_http_error_code;
		m_server_http_string = e.m_http_error_msg;
	}
	
	write_handshake();
}

void server_session::write_handshake() {
	std::stringstream h;
	
	
	
	if (m_server_http_code == 101) {
		std::string server_key = get_client_header("Sec-WebSocket-Key");
		server_key += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

		SHA1		sha;
		uint32_t	message_digest[5];
		
		sha.Reset();
		sha << server_key.c_str();
	
		if (sha.Result(message_digest)){
			// convert sha1 hash bytes to network byte order because this sha1
			//  library works on ints rather than bytes
			for (int i = 0; i < 5; i++) {
				message_digest[i] = htonl(message_digest[i]);
			}
	
			server_key = base64_encode(
					reinterpret_cast<const unsigned char*>(message_digest),20);
			
			// set handshake accept headers
			set_header("Sec-WebSocket-Accept",server_key);
			set_header("Upgrade","websocket");
			set_header("Connection","Upgrade");
		} else {
			log("Error computing handshake sha1 hash.",LOG_ERROR);
			m_server_http_code = 500;
			m_server_http_string = "";
		}
	}
	
	// hardcoded server headers
	set_header("Server","WebSocket++/2011-09-25");

	h << "HTTP/1.1 " << m_server_http_code << " "
	  << (m_server_http_string != "" ? m_server_http_string : 
	                         lookup_http_error_string(m_server_http_code))
	  << "\r\n";
	
	header_list::iterator it;
	for (it = m_server_headers.begin(); it != m_server_headers.end(); it++) {
		h << it->first << ": " << it->second << "\r\n";
	}

	h << "\r\n";
	
	m_raw_server_handshake = h.str();

	// start async write to handle_write_handshake
	boost::asio::async_write(
		m_socket,
		boost::asio::buffer(m_raw_server_handshake),
		boost::bind(
			&session::handle_write_handshake,
			shared_from_this(),
			boost::asio::placeholders::error
		)
	);
}

void server_session::handle_write_handshake(const boost::system::error_code& error) {
	if (error) {
		log_error("Error writing handshake response",error);
		drop_tcp();
		return;
	}
	
	log_open_result();

	if (m_server_http_code != 101) {
		std::stringstream err;
		err << "Handshake ended with HTTP error: " << m_server_http_code << " "
		    << (m_server_http_string != "" ? m_server_http_string : lookup_http_error_string(m_server_http_code));
		log(err.str(),LOG_ERROR);
		drop_tcp();
		// TODO: tell client that connection failed.
		return;
	}
	
	m_state = STATE_OPEN;
	
	// stop the handshake timer
	m_timer.cancel();
	
	if (m_local_interface) {
		m_local_interface->on_open(shared_from_this());
	}
	
	reset_message();
	this->read_frame();
}

void server_session::log(const std::string& msg, uint16_t level) const {
	m_server->log(msg,level);
}

void server_session::access_log(const std::string& msg, uint16_t level) const {
	m_server->access_log(msg,level);
}
