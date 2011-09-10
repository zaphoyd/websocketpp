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

#include "websocket_session.hpp"

#include "websocket_frame.hpp"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string.hpp>

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

using websocketpp::session;

session::session (boost::asio::io_service& io_service,
				  connection_handler_ptr defc)
	: m_socket(io_service),
	  m_status(CONNECTING),
	  m_local_interface(defc) {}

tcp::socket& session::socket() {
	return m_socket;
}

void session::start() {
	std::cout << "[Connection " << this << "] WebSocket Connection request from " << m_socket.remote_endpoint() << std::endl;
	
	// async read to handle_read_handshake
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

void session::set_handler(connection_handler_ptr new_con) {
	if (m_local_interface) {
		m_local_interface->disconnect(shared_from_this(),"Setting new connection handler");
	}
	m_local_interface = new_con;
	m_local_interface->connect(shared_from_this());
}

void session::add_host(std::string host) {
	m_hosts.insert(host);
}

void session::remove_host(std::string host) {
	m_hosts.erase(host);
}

void session::set_max_message_size(uint64_t val) {
	if (val > frame::PAYLOAD_64BIT_LIMIT) {
		throw "illegal maximum message size";
	}
	m_max_message_size = val;
}

std::string session::get_header(const std::string& key) const {
	std::map<std::string,std::string>::const_iterator h = m_headers.find(key);
	
	if (h == m_headers.end()) {
		return std::string();
	} else {
		return h->second;
	}
}

void session::add_header(const std::string &key,const std::string &value) {
	throw "unimplimented";
}

std::string session::get_request() const {
	return m_request;
}

std::string session::get_origin() const {
	if (m_version < 13) {
		return get_header("Sec-WebSocket-Origin");
	} else {
		return get_header("Origin");
	}
}

std::string session::lookup_http_error_string(int code) {
	switch (code) {
		case 400:
			return "Bad Request";
		case 401:
			return "Unauthorized";
		case 403:
			return "Forbidden";
		case 404:
			return "Not Found";
		case 405:
			return "Method Not Allowed";
		case 406:
			return "Not Acceptable";
		case 407:
			return "Proxy Authentication Required";
		case 408:
			return "Request Timeout";
		case 409:
			return "Conflict";
		case 410:
			return "Gone";
		case 411:
			return "Length Required";
		case 412:
			return "Precondition Failed";
		case 413:
			return "Request Entity Too Large";
		case 414:
			return "Request-URI Too Long";
		case 415:
			return "Unsupported Media Type";
		case 416:
			return "Requested Range Not Satisfiable";
		case 417:
			return "Expectation Failed";
		case 500:
			return "Internal Server Error";
		case 501:
			return "Not Implimented";
		case 502:
			return "Bad Gateway";
		case 503:
			return "Service Unavailable";
		case 504:
			return "Gateway Timeout";
		case 505:
			return "HTTP Version Not Supported";
		default:
			return "Unknown";
	}
}

void session::send(const std::string &msg) {
	m_write_frame.set_fin(true);
	m_write_frame.set_opcode(frame::TEXT_FRAME);
	m_write_frame.set_payload(msg);
	
	write_frame();
}

// send binary frame
void session::send(const std::vector<unsigned char> &data) {
	m_write_frame.set_fin(true);
	m_write_frame.set_opcode(frame::BINARY_FRAME);
	m_write_frame.set_payload(data);
	
	write_frame();
}

// send close frame
void session::disconnect(const std::string &reason) {
	m_write_frame.set_fin(true);
	m_write_frame.set_opcode(frame::CONNECTION_CLOSE);
	m_write_frame.set_payload(reason);
	
	write_frame();
	
	if (m_local_interface) {
		m_local_interface->disconnect(shared_from_this(),reason);
	}
}

void session::ping(const std::string &msg) {
	m_write_frame.set_fin(true);
	m_write_frame.set_opcode(frame::PING);
	m_write_frame.set_payload(msg);
	
	write_frame();
}

void session::pong(const std::string &msg) {
	m_write_frame.set_fin(true);
	m_write_frame.set_opcode(frame::PONG);
	m_write_frame.set_payload(msg);
	
	write_frame();
}

void session::handle_read_handshake(const boost::system::error_code& e,
	std::size_t bytes_transferred) {
	// read handshake and set local state (or pass to write_handshake)
	std::ostringstream line;
	line << &m_buf;
	m_handshake += line.str();
	
	//std::cout << "=== Raw Message ===" << std::endl;
	//std::cout << m_handshake << std::endl;
	//std::cout << "=== Raw Message end ===" << std::endl;
	
	std::vector<std::string> tokens;
	std::string::size_type start = 0;
	std::string::size_type end;
	
	// Get request and parse headers
	end = m_handshake.find("\r\n",start);
	
	while(end != std::string::npos) {
		tokens.push_back(m_handshake.substr(start, end - start));
		
		start = end + 2;
		
		end = m_handshake.find("\r\n",start);
	}
	
	for (size_t i = 0; i < tokens.size(); i++) {
		if (i == 0) {
			m_request = tokens[i];
		}
		
		end = tokens[i].find(": ",0);
		
		if (end != std::string::npos) {
			m_headers[tokens[i].substr(0,end)] = tokens[i].substr(end+2);
		}
	}
	
	// handshake error checking
	try {
		std::stringstream err;
		std::string h;
		
		// check the method
		if (m_request.substr(0,4) != "GET ") {
			err << "Websocket handshake has invalid method: "
				<< m_request.substr(0,4);
			
			throw(handshake_error(err.str(),400));
		}
		
		// check the HTTP version
		// TODO: allow versions greater than 1.1
		end = m_request.find(" HTTP/1.1",4);
		if (end == std::string::npos) {
			err << "Websocket handshake has invalid HTTP version";
			throw(handshake_error(err.str(),400));
		}
		
		m_request = m_request.substr(4,end-4);
		
		// verify the presence of required headers
		h = get_header("Host");
		if (h == "") {
			throw(handshake_error("Required Host header is missing",400));
		} else if (m_hosts.find(h) == m_hosts.end()) {
			err << "Host " << h << " is not one of this server's names.";
			throw(handshake_error(err.str(),400));
		}
		
		h = get_header("Upgrade");
		if (h == "") {
			throw(handshake_error("Required Upgrade header is missing",400));
		} else if (!boost::iequals(h,"websocket")) {
			err << "Upgrade header was " << h << " instead of \"websocket\"";
			throw(handshake_error(err.str(),400));
		}
		
		h = get_header("Connection");
		if (h == "") {
			throw(handshake_error("Required Connection header is missing",400));
		} else if (!boost::ifind_first(h,"upgrade")) {
			err << "Connection header, \"" << h 
				<< "\", does not contain required token \"upgrade\"";
			throw(handshake_error(err.str(),400));
		}
		
		if (get_header("Sec-WebSocket-Key") == "") {
			throw(handshake_error("Required Sec-WebSocket-Key header is missing",400));
		}
		
		h = get_header("Sec-WebSocket-Version");
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
		
		// optional headers (delegated to the local interface)
		if (m_local_interface) {
			m_local_interface->validate(shared_from_this());
		}
		
	} catch (const handshake_error& e) {
		std::cerr << "Caught handshake exception: " << e.what() << std::endl;
		e.write(shared_from_this());
		return;
	}
	
	this->write_handshake();
}

void session::write_handshake() {
	std::string server_handshake = "";
	std::string server_key = m_headers["Sec-WebSocket-Key"];
	server_key += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	
	SHA1		sha;
	uint32_t	message_digest[5];
	
	sha.Reset();
	sha << server_key.c_str();
	
	if (!sha.Result(message_digest)) {
		std::cerr << "Error computing sha1 hash, killing connection." << std::endl;
		write_http_error(500,"");
		return;
	}
	
	// convert sha1 hash bytes to network byte order because this sha1
	//  library works on ints rather than bytes
	for (int i = 0; i < 5; i++) {
		message_digest[i] = htonl(message_digest[i]);
	}
	
	server_key = base64_encode(
					reinterpret_cast<const unsigned char*>(message_digest),20);
	
	server_handshake += "HTTP/1.1 101 Switching Protocols\r\n";
	server_handshake += "Upgrade: websocket\r\n";
	server_handshake += "Connection: Upgrade\r\n";
	server_handshake += "Sec-WebSocket-Accept: "+server_key+"\r\n\r\n";
	
	// TODO: handler requested headers
		
	// start async write to handle_write_handshake
	boost::asio::async_write(
		m_socket,
		boost::asio::buffer(server_handshake),
		boost::bind(
			&session::handle_write_handshake,
			shared_from_this(),
			boost::asio::placeholders::error
		)
	);
}

void session::handle_write_handshake(const boost::system::error_code& error) {
	if (error) {
		handle_error("Error writing handshake",error);
		return;
	}
	
	std::cout << "WebSocket Version " << m_version << " connection opened." << std::endl;
	
	m_status = OPEN;
	
	if (m_local_interface) {
		m_local_interface->connect(shared_from_this());
	}
	
	reset_message();
	this->read_frame();
}

void session::write_http_error(int code,const std::string &msg) {
	std::stringstream server_handshake;
		
	server_handshake << "HTTP/1.1 " << code << " " 
					 << (msg != "" ? msg : lookup_http_error_string(code)) 
					 << "\r\n";
	
	// additional headers?
	
	server_handshake << "\r\n";
	
	// start async write to handle_write_handshake
	boost::asio::async_write(
		m_socket,
		boost::asio::buffer(server_handshake.str()),
		boost::bind(
			&session::handle_write_http_error,
			shared_from_this(),
			boost::asio::placeholders::error
		)
	);
}

void session::handle_write_http_error(const boost::system::error_code& error) {
	if (error) {
		handle_error("Error writing http response",error);
		return;
	}
}

void session::read_frame() {
	boost::asio::async_read(
		m_socket,
		boost::asio::buffer(m_read_frame.get_header(),
			frame::BASIC_HEADER_LENGTH),
		boost::bind(
			&session::handle_frame_header,
			shared_from_this(),
			boost::asio::placeholders::error
		)
	);
}



void session::handle_frame_header(const boost::system::error_code& error) {
	if (error) {
		handle_error("Error reading basic frame header",error);
		return;
	}
	
	uint16_t extended_header_bytes = m_read_frame.process_basic_header();

	if (!m_read_frame.validate_basic_header()) {
		handle_error("Basic header validation failed",boost::system::error_code());
		return;
	}

	if (extended_header_bytes == 0) {
		read_payload();
	} else {
		boost::asio::async_read(
			m_socket,
			boost::asio::buffer(m_read_frame.get_extended_header(),
				extended_header_bytes),
			boost::bind(
				&session::handle_extended_frame_header,
				shared_from_this(),
				boost::asio::placeholders::error
			)
		);
	}
}

void session::handle_extended_frame_header(
									const boost::system::error_code& error) {
	if (error) {
		handle_error("Error reading extended frame header",error);
		return;
	}
	
	// this sets up the buffer we are about to read into.
	m_read_frame.process_extended_header();
	
	this->read_payload();
}

void session::read_payload() {
	/*char * foo = m_read_frame.get_header();
	
	std::cout << std::hex << ((uint16_t*)foo)[0] << std::endl;
	
	std::cout << "opcode: " << m_read_frame.get_opcode() << std::endl;
	std::cout << "fin: " << m_read_frame.get_fin() << std::endl;
	std::cout << "mask: " << m_read_frame.get_masked() << std::endl;
	std::cout << "size: " << (uint16_t)m_read_frame.get_basic_size() << std::endl;
	std::cout << "payload_size: " << m_read_frame.get_payload_size() << std::endl;*/
	
	boost::asio::async_read(
		m_socket,
		boost::asio::buffer(m_read_frame.get_payload()),
		boost::bind(
			&session::handle_read_payload,
			shared_from_this(),
			boost::asio::placeholders::error
		)
	);
}

void session::handle_read_payload (const boost::system::error_code& error) {
	if (error) {
		handle_error("Error reading payload data frame header",error);
		return;
	}
	
	m_read_frame.process_payload();
	
	switch (m_read_frame.get_opcode()) {
		case frame::CONTINUATION_FRAME:
			process_continuation();
			break;
		case frame::TEXT_FRAME:
			process_text();
			break;
		case frame::BINARY_FRAME:
			process_binary();
			break;
		case frame::CONNECTION_CLOSE:
			process_close();
			break;
		case frame::PING:
			process_ping();
			break;
		case frame::PONG:
			process_pong();
			break;
		default:
			// TODO: unknown frame type
			// ignore or close?
			break;
	}
	
	// check if there was an error processing this frame and fail the connection
	if (m_error) {
		return;
	}
	
	this->read_frame();
}

void session::handle_write_frame (const boost::system::error_code& error) {
	if (error) {
		handle_error("Error writing frame data",error);
	}
	
	//std::cout << "Successfully wrote frame." << std::endl;
}

void session::process_ping() {
	std::cout << "Got ping" << std::endl;
	
	// send pong
	m_write_frame.set_fin(true);
	m_write_frame.set_opcode(frame::PONG);
	m_write_frame.set_payload(m_read_frame.get_payload());
	
	write_frame();
}

void session::process_pong() {
	std::cout << "Got pong" << std::endl;
}

void session::process_text() {
	// text is binary.
	process_binary();
}

void session::process_binary() {
	if (m_fragmented) {
		handle_error("Got a new message before the previous was finished.",
			boost::system::error_code());
			return;
	}
	
	m_current_opcode = m_read_frame.get_opcode();
	
	if (m_read_frame.get_fin()) {
		deliver_message();
		reset_message();
	} else {
		m_fragmented = true;
		extract_payload();
	}
}

void session::process_continuation() {
	if (!m_fragmented) {
		handle_error("Got a continuation frame without an outstanding message.",
			boost::system::error_code());
			return;
	}
	
	extract_payload();
	
	// check if we are done
	if (m_read_frame.get_fin()) {
		deliver_message();
		reset_message();
	}
}

void session::process_close() {
	if (m_status == OPEN) {
		// send response and set to closed
		std::string msg(m_read_frame.get_payload().begin(),
						m_read_frame.get_payload().end());
		
		std::cout << "Got connection close message, acking and closing the connection. Reason was: " << msg << std::endl;
		
		m_status = CLOSED;
		
		// send acknowledgement
		m_write_frame.set_fin(true);
		m_write_frame.set_opcode(frame::CONNECTION_CLOSE);
		m_write_frame.set_payload("");
	
		write_frame();
		
		// let our local interface know that the remote client has 
		// disconnected
		if (m_local_interface) {
			m_local_interface->disconnect(shared_from_this(),msg);
		}
	} else if (m_status == CLOSING) {
		// this is an ack of my close message
		// close cleanly
		std::cout << "Got ack for my close message, closing the connection" << std::endl;
		m_status = CLOSED;
		
		// let our local interface know that the remote client has 
		// disconnected and the reason (if any)
		if (m_local_interface) {
			m_local_interface->disconnect(shared_from_this(),"");
		}
	} else {
		// ignore
	}
}

void session::deliver_message() {
	if (!m_local_interface) {
		return;
	}
	
	if (m_current_opcode == frame::BINARY_FRAME) {
		if (m_fragmented) {
			m_local_interface->message(shared_from_this(),m_current_message);
		} else {
			m_local_interface->message(shared_from_this(),
									   m_read_frame.get_payload());
		}
	} else if (m_current_opcode == frame::TEXT_FRAME) {
		std::string msg;
		
		if (m_fragmented) {
			msg.append(m_current_message.begin(),m_current_message.end());
		} else {
			msg.append(
				m_read_frame.get_payload().begin(),
				m_read_frame.get_payload().end()
			);
		}
		
		m_local_interface->message(shared_from_this(),msg);
	} else {
		// fail
	}
	
}

void session::extract_payload() {
	std::vector<unsigned char> &msg = m_read_frame.get_payload();
	m_current_message.resize(m_current_message.size()+msg.size());
	std::copy(msg.begin(),msg.end(),m_current_message.end()-msg.size());
}

void session::write_frame() {
	// print debug info
	m_write_frame.print_frame();
	
	std::vector<boost::asio::mutable_buffer> data;
	
	data.push_back(
		boost::asio::buffer(
			m_write_frame.get_header(),
			m_write_frame.get_header_len()
		)
	);
	data.push_back(
		boost::asio::buffer(m_write_frame.get_payload())
	);
	
	boost::asio::async_write(
		m_socket,
		data,
		boost::bind(
			&session::handle_write_frame,
			shared_from_this(),
			boost::asio::placeholders::error
		)
	);
}

void session::reset_message() {
	m_error = false;
	m_fragmented = false;
	m_current_message.clear();
}

void session::handle_error(std::string msg,
						   const boost::system::error_code& error) {
	std::stringstream e;
	
	e << msg << " (" << error << ")";
	
	std::cerr << e.str() << std::endl;
	
	if (m_local_interface) {
		m_local_interface->disconnect(shared_from_this(),e.str());
	}
	
	m_error = true;
}