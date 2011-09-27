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

session::session (boost::asio::io_service& io_service,
				  connection_handler_ptr defc)
	: m_status(CONNECTING),
	  m_local_close_code(CLOSE_STATUS_NO_STATUS),
	  m_remote_close_code(CLOSE_STATUS_NO_STATUS),
	  m_was_clean(false),
	  m_closed_by_me(false),
	  m_dropped_by_me(false),
	  m_socket(io_service),
	  m_io_service(io_service),
	  m_local_interface(defc),
	  
	  
	  m_utf8_state(utf8_validator::UTF8_ACCEPT),
	  m_utf8_codepoint(0) {}

tcp::socket& session::socket() {
	return m_socket;
}

boost::asio::io_service& session::io_service() {
	return m_io_service;
}

void session::set_handler(connection_handler_ptr new_con) {
	if (m_local_interface) {
		// TODO: this should be another method and not reusing onclose
		//m_local_interface->disconnect(shared_from_this(),4000,"Setting new connection handler");
	}
	m_local_interface = new_con;
	m_local_interface->on_open(shared_from_this());
}

const std::string& session::get_subprotocol() const {
	if (m_status == CONNECTING) {
		log("Subprotocol is not avaliable before the handshake has completed.",LOG_WARN);
		throw server_error("Subprotocol is not avaliable before the handshake has completed.");
	}
	return m_server_subprotocol;
}

const std::string& session::get_resource() const {
	return m_resource;
}

const std::string& session::get_origin() const {
	return m_client_origin;
}

std::string session::get_client_header(const std::string& key) const {
	return get_header(key,m_client_headers);
}

std::string session::get_server_header(const std::string& key) const {
	return get_header(key,m_server_headers);
}

std::string session::get_header(const std::string& key,
                                const header_list& list) const {
	header_list::const_iterator h = list.find(key);
	
	if (h == list.end()) {
		return "";
	} else {
		return h->second;
	}
}

const std::vector<std::string>& session::get_extensions() const {
	return m_server_extensions;
}

unsigned int session::get_version() const {
	return m_version;
}

void session::send(const std::string &msg) {
	if (m_status != OPEN) {
		log("Tried to send a message from a session that wasn't open",LOG_WARN);
		return;
	}
	m_write_frame.set_fin(true);
	m_write_frame.set_opcode(frame::TEXT_FRAME);
	m_write_frame.set_payload(msg);
	
	write_frame();
}

void session::send(const std::vector<unsigned char> &data) {
	if (m_status != OPEN) {
		log("Tried to send a message from a session that wasn't open",LOG_WARN);
		return;
	}
	m_write_frame.set_fin(true);
	m_write_frame.set_opcode(frame::BINARY_FRAME);
	m_write_frame.set_payload(data);
	
	write_frame();
}

void session::close(uint16_t status,const std::string& msg) {
	disconnect(status,msg);
	// TODO: close behavior
}

// TODO: clean this up, needs to be broken out into more specific methods
void session::disconnect(uint16_t status,const std::string &message) {
	if (m_status != OPEN) {
		log("Tried to disconnect a session that wasn't open",LOG_WARN);
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
		log("Tried to disconnect with status ABNORMAL_CLOSE",LOG_DEBUG);
	} else {
		m_write_frame.set_status(status,message);
	}

	write_frame();
}

void session::ping(const std::string &msg) {
	if (m_status != OPEN) {
		log("Tried to send a ping from a session that wasn't open",LOG_WARN);
		return;
	}
	m_write_frame.set_fin(true);
	m_write_frame.set_opcode(frame::PING);
	m_write_frame.set_payload(msg);
	
	write_frame();
}

void session::pong(const std::string &msg) {
	if (m_status != OPEN) {
		log("Tried to send a pong from a session that wasn't open",LOG_WARN);
		return;
	}
	m_write_frame.set_fin(true);
	m_write_frame.set_opcode(frame::PONG);
	m_write_frame.set_payload(msg);
	
	write_frame();
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
		// TODO: close behavior
		return;	
	}
	log(m_read_frame.print_frame(),LOG_DEBUG);
	
	uint16_t extended_header_bytes = m_read_frame.process_basic_header();

	if (!m_read_frame.validate_basic_header()) {
		handle_error("Basic header validation failed",boost::system::error_code());
		disconnect(CLOSE_STATUS_PROTOCOL_ERROR,"");
		
		
		// TODO: close behavior
		return;
	}

	if (extended_header_bytes == 0) {
		m_read_frame.process_extended_header();
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
		// TODO: close behavior
		return;
	}
	
	// this sets up the buffer we are about to read into.
	m_read_frame.process_extended_header();
	
	this->read_payload();
}

void session::read_payload() {
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
		// TODO: close behavior
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
				// TODO: close behavior
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
		// TODO: close behavior
		return;
	}

	// check if there was an error processing this frame and fail the connection
	if (m_error) {
		log("Connection has been closed uncleanly",LOG_ERROR);
		// TODO: close behavior
		return;
	}
	
	if (m_status == CLOSED) {
		log_close_result();

		if (m_local_interface) {
			m_local_interface->on_close(shared_from_this(),
			                            m_close_code,
			                            m_close_message);
		}
		// TODO: close behavior
		return;
	}

	this->read_frame();
}

void session::handle_write_frame (const boost::system::error_code& error) {
	if (error) {
		handle_error("Error writing frame data",error);
		// TODO: close behavior
	}
	
	//std::cout << "Successfully wrote frame." << std::endl;
}

void session::process_ping() {
	access_log("Ping",ALOG_MISC_CONTROL);
	// TODO: on_ping

	// send pong
	m_write_frame.set_fin(true);
	m_write_frame.set_opcode(frame::PONG);
	m_write_frame.set_payload(m_read_frame.get_payload());
	
	write_frame();
}

void session::process_pong() {
	access_log("Pong",ALOG_MISC_CONTROL);
	// TODO: on_pong
}

void session::process_text() {
	if (!m_read_frame.validate_utf8(&m_utf8_state,&m_utf8_codepoint)) {
		disconnect(CLOSE_STATUS_INVALID_PAYLOAD,"Invalid UTF8 Data");	
		// TODO: close behavior
		return;
	}

	process_binary();
}

void session::process_binary() {
	if (m_fragmented) {
		handle_error("Got a new message before the previous was finished.",
			boost::system::error_code());
			disconnect(CLOSE_STATUS_PROTOCOL_ERROR,"");
			// TODO: close behavior
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
			// TODO: close behavior
			return;
	}
	
	if (m_current_opcode == frame::TEXT_FRAME) {
		if (!m_read_frame.validate_utf8(&m_utf8_state,&m_utf8_codepoint)) {
			disconnect(CLOSE_STATUS_INVALID_PAYLOAD,"Invalid UTF8 Data");	
			// TODO: close behavior
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
		// TODO: close behavior
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
			m_local_interface->on_message(shared_from_this(),m_current_message);
		} else {
			m_local_interface->on_message(shared_from_this(),
									   m_read_frame.get_payload());
		}
	} else if (m_current_opcode == frame::TEXT_FRAME) {
		std::string msg;
		
		// make sure the finished frame is valid utf8
		if (m_utf8_state != utf8_validator::UTF8_ACCEPT) {
			disconnect(CLOSE_STATUS_INVALID_PAYLOAD,"Invalid UTF8 Data");
			// TODO: close behavior
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
		
		m_local_interface->on_message(shared_from_this(),msg);
	} else {
		// Not sure if this should be a fatal error or not
		std::stringstream err;
		err << "Attempted to deliver a message of unsupported opcode " << m_current_opcode;
		log(err.str(),LOG_ERROR);
	}
	
}

void session::extract_payload() {
	std::vector<unsigned char> &msg = m_read_frame.get_payload();
	m_current_message.resize(m_current_message.size()+msg.size());
	std::copy(msg.begin(),msg.end(),m_current_message.end()-msg.size());
}

void session::write_frame() {
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
	
	log("Write Frame: "+m_write_frame.print_frame(),LOG_DEBUG);

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

void session::log_close_result() {
	std::stringstream msg;

	msg << "[Connection " << this << "] "
		<< (m_was_clean ? "Clean " : "Unclean ")
		<< "close local:[" << m_local_close_code
		<< (m_local_close_msg == "" ? "" : ","+m_local_close_msg) 
		<< "] remote:[" << m_remote_close_code 
		<< (m_remote_close_msg == "" ? "" : ","+m_remote_close_msg) << "]";
	
	access_log(msg.str(),ALOG_DISCONNECT);
}

void session::log_open_result() {
	std::stringstream msg;
	
	msg << "[Connection " << this << "] "
	    << m_socket.remote_endpoint()
	    << " v" << m_version << " "
	    << (get_client_header("User-Agent") == "" ? "NULL" : get_client_header("User-Agent")) 
	    << " " << m_resource << " " << m_server_http_code;
	
	access_log(msg.str(),ALOG_HANDSHAKE);
}

void session::handle_error(std::string msg,
						   const boost::system::error_code& error) {
	std::stringstream e;
	
	e << "[Connection " << this << "] " << msg << " (" << error << ")";
	
	log(e.str(),LOG_ERROR);
		
	if (m_local_interface) {
		m_local_interface->on_close(shared_from_this(),1006,e.str());
	}
	
	m_error = true;
}
