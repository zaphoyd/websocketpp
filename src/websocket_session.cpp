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
#include "utf8_validator/utf8_validator.hpp"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string.hpp>


#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

using websocketpp::session;

session::session (server_ptr s,boost::asio::io_service& io_service,
				  connection_handler_ptr defc)
	: m_status(CONNECTING),
	  m_local_close_code(CLOSE_STATUS_NO_STATUS),
	  m_remote_close_code(CLOSE_STATUS_NO_STATUS),
	  m_was_clean(false),
	  m_closed_by_me(false),
	  m_dropped_by_me(false),
	  m_server(s),
	  m_socket(io_service),
	  m_local_interface(defc),
	  
	  
	  m_utf8_state(utf8_validator::UTF8_ACCEPT),
	  m_utf8_codepoint(0) {}

tcp::socket& session::socket() {
	return m_socket;
}

void session::start() {
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
		m_local_interface->disconnect(shared_from_this(),4000,"Setting new connection handler");
	}
	m_local_interface = new_con;
	m_local_interface->connect(shared_from_this());
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

void session::send(const std::string &msg) {
	if (m_status != OPEN) {
		// error?
		return;
	}
	m_write_frame.set_fin(true);
	m_write_frame.set_opcode(frame::TEXT_FRAME);
	m_write_frame.set_payload(msg);
	
	write_frame();
}

// send binary frame
void session::send(const std::vector<unsigned char> &data) {
	if (m_status != OPEN) {
		// error?
		return;
	}
	m_write_frame.set_fin(true);
	m_write_frame.set_opcode(frame::BINARY_FRAME);
	m_write_frame.set_payload(data);
	
	write_frame();
}

// send close frame
void session::disconnect(uint16_t status,const std::string &message) {
	if (m_status != OPEN) {
		m_server->error_log("got a disconnect call from invalid state");
		return;
	}

	m_status = CLOSING;
	
	m_close_code = status;
	m_close_message = message;
	
	m_local_close_code = status;
	m_local_close_msg = message;

	m_write_frame.set_fin(true);
	m_write_frame.set_opcode(frame::CONNECTION_CLOSE);
	
	if (status == CLOSE_STATUS_NO_STATUS) {
		m_write_frame.set_status(CLOSE_STATUS_NORMAL,"");
	} else if (status == CLOSE_STATUS_ABNORMAL_CLOSE) {
		// unknown internal error, don't set a status? use protocol error?
	} else {
		m_write_frame.set_status(status,message);
	}

	write_frame();
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
		} else if (!m_server->validate_host(h)) {
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
		std::stringstream err;
		err << "Caught handshake exception: " << e.what();
		
		m_server->error_log(err.str());
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
		m_server->error_log("Error computing handshake sha1 hash.");
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
	server_handshake += "Sec-WebSocket-Accept: "+server_key+"\r\n";
	server_handshake += "Server: WebSocket++/2011-09-22\r\n\r\n";

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
	
	access_log_open(101);
	
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
					 << "\r\n"
					 << "Server: WebSocket++/2011-09-22\r\n";
	
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
	
	access_log_open(code);

	std::stringstream err;
	err << "Handshake ended with HTTP error: " << code << " " 
		<< (msg != "" ? msg : lookup_http_error_string(code)); 
	
	m_server->error_log(err.str());
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
		disconnect(CLOSE_STATUS_PROTOCOL_ERROR,"");
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
	

	if (m_status == OPEN) {
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
				disconnect(CLOSE_STATUS_PROTOCOL_ERROR,"Invalid Opcode");
				break;
		}
	} else if (m_status == CLOSING) {
		if (m_read_frame.get_opcode() == frame::CONNECTION_CLOSE) {
			process_close();
		} else {
			// Ignore all other frames in closing state
		}
	} else {
		// Recieved message before or after connection was opened/closed
		return;
	}

	// check if there was an error processing this frame and fail the connection
	if (m_error) {
		m_server->error_log("Connection has been closed uncleanly");
		return;
	}
	
	if (m_status == CLOSED) {
		access_log_close();

		if (m_local_interface) {
			m_local_interface->disconnect(shared_from_this(),
			                              m_close_code,
										  m_close_message);
		}
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
	m_server->access_log("Ping");
	
	// send pong
	m_write_frame.set_fin(true);
	m_write_frame.set_opcode(frame::PONG);
	m_write_frame.set_payload(m_read_frame.get_payload());
	
	write_frame();
}

void session::process_pong() {
	m_server->access_log("Pong");
}

void session::process_text() {
	if (!m_read_frame.validate_utf8(&m_utf8_state,&m_utf8_codepoint)) {
		disconnect(CLOSE_STATUS_INVALID_PAYLOAD,"Invalid UTF8 Data");	
		return;
	}

	process_binary();
}

void session::process_binary() {
	if (m_fragmented) {
		handle_error("Got a new message before the previous was finished.",
			boost::system::error_code());
			disconnect(CLOSE_STATUS_PROTOCOL_ERROR,"");
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
			disconnect(CLOSE_STATUS_PROTOCOL_ERROR,"");
			return;
	}
	
	if (m_current_opcode == frame::TEXT_FRAME) {
		if (!m_read_frame.validate_utf8(&m_utf8_state,&m_utf8_codepoint)) {
			disconnect(CLOSE_STATUS_INVALID_PAYLOAD,"Invalid UTF8 Data");	
			return;
		}
	}

	extract_payload();
	
	// check if we are done
	if (m_read_frame.get_fin()) {
		deliver_message();
		reset_message();
	}
}

void session::process_close() {
	uint16_t status = m_read_frame.get_close_status();
	std::string message = m_read_frame.get_close_msg();
	
	m_remote_close_code = status;
	m_remote_close_msg = message;


	if (m_status == OPEN) {
		// This is the case where the remote initiated the close.
		m_closed_by_me = false;
		disconnect(status,message);
	} else if (m_status == CLOSING) {
		// this is an ack of our close message
		m_closed_by_me = true;
	} else {
		throw "fixme";
	}

	m_was_clean = true;
	m_status = CLOSED;
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
		
		// make sure the finished frame is valid utf8
		if (m_utf8_state != utf8_validator::UTF8_ACCEPT) {
			disconnect(CLOSE_STATUS_INVALID_PAYLOAD,"Invalid UTF8 Data");
			return;
		}

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
		// Not sure if this should be a fatal error or not
		std::stringstream err;
		err << "Attempted to deliver a message of unsupported opcode " << m_current_opcode;
		m_server->error_log(err.str());
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

	m_utf8_state = utf8_validator::UTF8_ACCEPT;
	m_utf8_codepoint = 0;
}

void session::access_log_close() {
	std::stringstream msg;

	msg << "[Connection " << this << "] "
		<< (m_was_clean ? "Clean " : "Unclean ")
		<< "close local:[" << m_local_close_code
		<< (m_local_close_msg == "" ? "" : ","+m_local_close_msg) 
		<< "] remote:[" << m_remote_close_code 
		<< (m_remote_close_msg == "" ? "" : ","+m_remote_close_msg) << "]";
	
	m_server->access_log(msg.str());
}

void session::access_log_open(int code) {
	std::stringstream msg;
	
	msg << "[Connection " << this << "] "
	    << m_socket.remote_endpoint()
	    << " v" << m_version << " "
	    << (get_header("User-Agent") == "" ? "NULL" : get_header("User-Agent")) 
	    << " " << m_request << " " << code;
	
	m_server->access_log(msg.str());
}

void session::handle_error(std::string msg,
						   const boost::system::error_code& error) {
	std::stringstream e;
	
	e << "[Connection " << this << "] " << msg << " (" << error << ")";
	
	m_server->error_log(e.str());
		
	if (m_local_interface) {
		m_local_interface->disconnect(shared_from_this(),1006,e.str());
	}
	
	m_error = true;
}
