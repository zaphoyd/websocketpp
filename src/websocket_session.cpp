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
				  websocketpp::connection_handler_ptr defc,
				  uint64_t buf_size)
	: m_state(STATE_CONNECTING),
	  m_writing(false),
	  m_local_close_code(CLOSE_STATUS_NO_STATUS),
	  m_remote_close_code(CLOSE_STATUS_NO_STATUS),
	  m_was_clean(false),
	  m_closed_by_me(false),
	  m_dropped_by_me(false),
	  m_socket(io_service),
	  m_io_service(io_service),
	  m_local_interface(defc),
	  m_timer(io_service,boost::posix_time::seconds(0)),
	  m_buf(buf_size), // maximum buffered (unconsumed) bytes from network
	  m_utf8_state(utf8_validator::UTF8_ACCEPT),
	  m_utf8_codepoint(0) {}

tcp::socket& session::socket() {
	return m_socket;
}

boost::asio::io_service& session::io_service() {
	return m_io_service;
}

void session::set_handler(websocketpp::connection_handler_ptr new_con) {
	if (m_local_interface) {
		// TODO: this should be another method and not reusing onclose
		//m_local_interface->disconnect(shared_from_this(),4000,"Setting new connection handler");
	}
	m_local_interface = new_con;
	m_local_interface->on_open(shared_from_this());
}

const std::string& session::get_subprotocol() const {
	if (m_state == STATE_CONNECTING) {
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
                                const websocketpp::header_list& list) const {
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
	if (m_state != STATE_OPEN) {
		log("Tried to send a message from a session that wasn't open",LOG_WARN);
		return;
	}
	m_write_frame.set_fin(true);
	m_write_frame.set_opcode(frame::TEXT_FRAME);
	m_write_frame.set_payload(msg);
	
	write_frame();
}

void session::send(const std::vector<unsigned char> &data) {
	if (m_state != STATE_OPEN) {
		log("Tried to send a message from a session that wasn't open",LOG_WARN);
		return;
	}
	m_write_frame.set_fin(true);
	m_write_frame.set_opcode(frame::BINARY_FRAME);
	m_write_frame.set_payload(data);
	
	write_frame();
}

// end user interface to close the connection
void session::close(uint16_t status,const std::string& msg) {
	validate_app_close_status(status);
	
	send_close(status,msg);
}

// TODO: clean this up, needs to be broken out into more specific methods

// This method initiates a clean disconnect with the given status code and reason
// it logs an error and is ignored if it is called from a state other than OPEN

// called by process_close when an initiate close method is received.

void session::send_close(uint16_t status,const std::string &message) {
	if (m_state != STATE_OPEN) {
		log("Tried to disconnect a session that wasn't open",LOG_WARN);
		return;
	}

	m_state = STATE_CLOSING;
	
	m_timer.expires_from_now(boost::posix_time::milliseconds(1000));
	
	m_timer.async_wait(
		boost::bind(
			&session::handle_close_expired,
			shared_from_this(),
			boost::asio::placeholders::error
		)
	);
	
	m_local_close_code = status;
	m_local_close_msg = message;

	m_write_frame.set_fin(true);
	m_write_frame.set_opcode(frame::CONNECTION_CLOSE);
	
	// echo close value unless there is a good reason not to.
	if (status == CLOSE_STATUS_NO_STATUS) {
		m_write_frame.set_status(CLOSE_STATUS_NORMAL,"");
	} else if (status == CLOSE_STATUS_ABNORMAL_CLOSE) {
		// Internal implimentation error. There is no good close code for this.
		m_write_frame.set_status(CLOSE_STATUS_POLICY_VIOLATION,message);
	} else if (close::status::invalid(status)) {
		m_write_frame.set_status(close::status::PROTOCOL_ERROR,"Status code is invalid");
	} else if (close::status::reserved(status)) {
		m_write_frame.set_status(close::status::PROTOCOL_ERROR,"Status code is reserved");
	} else {
		m_write_frame.set_status(status,message);
	}
		
	write_frame();
}

void session::ping(const std::string &msg) {
	if (m_state != STATE_OPEN) {
		log("Tried to send a ping from a session that wasn't open",LOG_WARN);
		return;
	}
	m_write_frame.set_fin(true);
	m_write_frame.set_opcode(frame::PING);
	m_write_frame.set_payload(msg);
	
	write_frame();
}

void session::pong(const std::string &msg) {
	if (m_state != STATE_OPEN) {
		log("Tried to send a pong from a session that wasn't open",LOG_WARN);
		return;
	}
	m_write_frame.set_fin(true);
	m_write_frame.set_opcode(frame::PONG);
	m_write_frame.set_payload(msg);
	
	write_frame();
}

void session::read_frame() {
	// the initial read in the handshake may have read in the first frame.
	// handle it (if it exists) before we read anything else.
	handle_read_frame(boost::system::error_code());
}

// handle_read_frame reads and processes all socket read commands for the 
// session by consuming the read buffer and then starting an async read with
// itself as the callback. The connection is over when this method returns.
void session::handle_read_frame(const boost::system::error_code& error) {
	if (m_state != STATE_OPEN && m_state != STATE_CLOSING) {
		log("handle_read_frame called in invalid state",LOG_ERROR);
		return;
	}
	
	if (error) {
		if (error == boost::asio::error::eof) {			
			// if this is a case where we are expecting eof, return, else log & drop
			
			log_error("Recieved EOF",error);
			//drop_tcp(false);
			//m_state = STATE_CLOSED;
		} else if (error == boost::asio::error::operation_aborted) {
			// some other part of our client called shutdown on our socket.
			// This is usually due to a write error. Everything should have 
			// already been logged and dropped so we just return here
			return;
		} else {
			log_error("Error reading frame",error);
			//drop_tcp(false);
			m_state = STATE_CLOSED;
		}
	}
	
	std::istream s(&m_buf);
	
	while (m_buf.size() > 0 && m_state != STATE_CLOSED) {
		try {
			if (m_read_frame.get_bytes_needed() == 0) {
				throw frame_error("have bytes that no frame needs",frame::FERR_FATAL_SESSION_ERROR);
			}
			
			// Consume will read bytes from s
			// will throw a frame_error on error.
			
			std::stringstream err;
			
			err << "consuming. have: " << m_buf.size() << " bytes. Need: " << m_read_frame.get_bytes_needed() << " state: " << (int)m_read_frame.get_state();
			log(err.str(),LOG_DEBUG);
			m_read_frame.consume(s);
			
			err.str("");
			err << "consume complete, " << m_buf.size() << " bytes left, " << m_read_frame.get_bytes_needed() << " still needed, state: " << (int)m_read_frame.get_state();
			log(err.str(),LOG_DEBUG);
			
			if (m_read_frame.get_state() == frame::STATE_READY) {
				// process frame and reset frame state for the next frame.
				// will throw a frame_error on error. May set m_state to CLOSED,
				// if so no more frames should be processed.
				err.str("");
				err << "processing frame " << m_buf.size();
				log(err.str(),LOG_DEBUG);
				m_timer.cancel();
				process_frame();
			}
		} catch (const frame_error& e) {
			std::stringstream err;
			err << "Caught frame exception: " << e.what();
			
			access_log(e.what(),ALOG_FRAME);
			log(err.str(),LOG_ERROR);
			
			// if the exception happened while processing.
			// TODO: this is not elegant, perhaps separate frame read vs process
			// exceptions need to be used.
			if (m_read_frame.get_state() == frame::STATE_READY) {
				m_read_frame.reset();
			}
			
			// process different types of frame errors
			// 
			if (e.code() == frame::FERR_PROTOCOL_VIOLATION) {
				send_close(CLOSE_STATUS_PROTOCOL_ERROR, e.what());
			} else if (e.code() == frame::FERR_PAYLOAD_VIOLATION) {
				send_close(CLOSE_STATUS_INVALID_PAYLOAD, e.what());
			} else if (e.code() == frame::FERR_INTERNAL_SERVER_ERROR) {
				send_close(CLOSE_STATUS_ABNORMAL_CLOSE, e.what());
			} else if (e.code() == frame::FERR_SOFT_SESSION_ERROR) {
				// ignore and continue processing frames
				continue;
			} else {
				// Fatal error, forcibly end connection immediately.
				log("Dropping TCP due to unrecoverable exception",LOG_DEBUG);
				drop_tcp(true);
			}
			
			break;
		}
	}
	
	if (error == boost::asio::error::eof) {			
		m_state = STATE_CLOSED;
	}
	
	// we have read everything, check if we should read more
	
	if ((m_state == STATE_OPEN || m_state == STATE_CLOSING) && m_read_frame.get_bytes_needed() > 0) {
		std::stringstream msg;
		msg << "starting async read for " << m_read_frame.get_bytes_needed() << " bytes.";
		
		log(msg.str(),LOG_DEBUG);
		
		// TODO: set a timer here in case we don't want to read forever. 
		// Ex: when the frame is in a degraded state.
		
		boost::asio::async_read(
			m_socket,
			m_buf,
			boost::asio::transfer_at_least(m_read_frame.get_bytes_needed()),
			boost::bind(
				&session::handle_read_frame,
				shared_from_this(),
				boost::asio::placeholders::error
			)
		);
	} else if (m_state == STATE_CLOSED) {
		log_close_result();
		
		if (m_local_interface) {
			// TODO: make sure close code/msg are properly set.
			m_local_interface->on_close(shared_from_this());
		}
		
		m_timer.cancel();
	} else {
		log("handle_read_frame called in invalid state",LOG_ERROR);
	}
}

void session::process_frame () {
	log("process_frame",LOG_DEBUG);
	
	if (m_state == STATE_OPEN) {
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
				log("process_close",LOG_DEBUG);
				process_close();
				break;
			case frame::PING:
				process_ping();
				break;
			case frame::PONG:
				process_pong();
				break;
			default:
				throw frame_error("Invalid Opcode",
								  frame::FERR_PROTOCOL_VIOLATION);
				break;
		}
	} else if (m_state == STATE_CLOSING) {
		if (m_read_frame.get_opcode() == frame::CONNECTION_CLOSE) {
			process_close();
		} else {
			// Ignore all other frames in closing state
			log("ignoring this frame",LOG_DEBUG);
		}
	} else {
		// Recieved message before or after connection was opened/closed
		throw frame_error("process_frame called from invalid state");
	}

	m_read_frame.reset();
}

void session::handle_write_frame (const boost::system::error_code& error) {
	if (error) {
		log_error("Error writing frame data",error);
		drop_tcp(false);
	}
	
	access_log("handle_write_frame complete",ALOG_FRAME);
	m_writing = false;
}


void session::handle_timer_expired (const boost::system::error_code& error) {
	if (error) {
		if (error == boost::asio::error::operation_aborted) {
			log("timer was aborted",LOG_DEBUG);
			//drop_tcp(false);
		} else {
			log("timer ended with error",LOG_DEBUG);
		}
		return;
	}
	
	log("timer ended without error",LOG_DEBUG);
	
	
}

void session::handle_handshake_expired (const boost::system::error_code& error) {
	if (error) {
		if (error != boost::asio::error::operation_aborted) {
			log("Unexpected handshake timer error.",LOG_DEBUG);
			drop_tcp(true);
		}
		return;
	}
	
	log("Handshake timed out",LOG_DEBUG);
	drop_tcp(true);
}

// The error timer is set when we want to give the other endpoint some time to
// do something but don't want to wait forever. There is a special error code
// that represents the timer being canceled by us (because the other endpoint
// responded in time. All other cases should assume that the other endpoint is
// irrepairibly broken and drop the TCP connection.
void session::handle_error_timer_expired (const boost::system::error_code& error) {
	if (error) {
		if (error == boost::asio::error::operation_aborted) {
			log("error timer was aborted",LOG_DEBUG);
			//drop_tcp(false);
		} else {
			log("error timer ended with error",LOG_DEBUG);
			drop_tcp(true);
		}
		return;
	}
	
	log("error timer ended without error",LOG_DEBUG);
	drop_tcp(true);
}

void session::handle_close_expired (const boost::system::error_code& error) {
	if (error) {
		if (error == boost::asio::error::operation_aborted) {
			log("timer was aborted",LOG_DEBUG);
			//drop_tcp(false);
		} else {
			log("Unexpected close timer error.",LOG_DEBUG);
			drop_tcp(false);
		}
		return;
	}
	
	if (m_state != STATE_CLOSED) {
		log("close timed out",LOG_DEBUG);
		drop_tcp(false);
	}
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
	// this will throw an exception if validation fails at any point
	m_read_frame.validate_utf8(&m_utf8_state,&m_utf8_codepoint);
	
	// otherwise, treat as binary
	process_binary();
}

void session::process_binary() {
	if (m_fragmented) {
		throw frame_error("Got a new message before the previous was finished.",
						  frame::FERR_PROTOCOL_VIOLATION);
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
		throw frame_error("Got a continuation frame without an outstanding message.",
						  frame::FERR_PROTOCOL_VIOLATION);
	}
	
	if (m_current_opcode == frame::TEXT_FRAME) {
		// this will throw an exception if validation fails at any point
		m_read_frame.validate_utf8(&m_utf8_state,&m_utf8_codepoint);
	}

	extract_payload();
	
	// check if we are done
	if (m_read_frame.get_fin()) {
		deliver_message();
		reset_message();
	}
}

void session::process_close() {
	m_remote_close_code = m_read_frame.get_close_status();
	m_remote_close_msg = m_read_frame.get_close_msg();

	if (m_state == STATE_OPEN) {
		log("process_close sending ack",LOG_DEBUG);
		// This is the case where the remote initiated the close.
		m_closed_by_me = false;
		// send acknowledgement
		
		// check if the remote close code
		if (m_remote_close_code == close::status::NO_STATUS) {
			send_close(close::status::NORMAL,"");
		} else if (close::status::invalid(m_remote_close_code)) {
			send_close(close::status::PROTOCOL_ERROR,"Invalid status code");
		} else if (close::status::reserved(m_remote_close_code)) {
			send_close(close::status::PROTOCOL_ERROR,"Reserved status code");
		} else {
			send_close(m_remote_close_code,m_remote_close_msg);
		}
	} else if (m_state == STATE_CLOSING) {
		log("process_close got ack",LOG_DEBUG);
		// this is an ack of our close message
		m_closed_by_me = true;
	} else {
		throw frame_error("process_closed called from wrong state");
	}

	m_was_clean = true;
	m_state = STATE_CLOSED;
}

void session::deliver_message() {
	if (!m_local_interface) {
		return;
	}
	
	if (m_current_opcode == frame::BINARY_FRAME) {
		//log("Dispatching Binary Message",LOG_DEBUG);
		if (m_fragmented) {
			m_local_interface->on_message(shared_from_this(),m_current_message);
		} else {
			m_local_interface->on_message(shared_from_this(),
									   m_read_frame.get_payload());
		}
	} else if (m_current_opcode == frame::TEXT_FRAME) {
		std::string msg;
		
		// make sure the finished frame is valid utf8
		// the streaming validator checks for bad codepoints as it goes. It 
		// doesn't know where the end of the message is though, so we need to 
		// check here to make sure the final message ends on a valid codepoint.
		if (m_utf8_state != utf8_validator::UTF8_ACCEPT) {
			throw frame_error("Invalid UTF-8 Data",
							  frame::FERR_PAYLOAD_VIOLATION);
		}

		if (m_fragmented) {
			msg.append(m_current_message.begin(),m_current_message.end());
		} else {
			msg.append(
				m_read_frame.get_payload().begin(),
				m_read_frame.get_payload().end()
			);
		}
		
		//log("Dispatching Text Message",LOG_DEBUG);
		m_local_interface->on_message(shared_from_this(),msg);
	} else {
		// Not sure if this should be a fatal error or not
		std::stringstream err;
		err << "Attempted to deliver a message of unsupported opcode " << m_current_opcode;
		throw frame_error(err.str(),frame::FERR_SOFT_SESSION_ERROR);
	}
	
}

void session::extract_payload() {
	std::vector<unsigned char> &msg = m_read_frame.get_payload();
	m_current_message.resize(m_current_message.size()+msg.size());
	std::copy(msg.begin(),msg.end(),m_current_message.end()-msg.size());
}

void session::write_frame() {
	if (!is_server()) {
		m_write_frame.set_masked(true); // client must mask frames
	}
	
	m_write_frame.process_payload();
	
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
	
	m_writing = true;
	
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
	
	access_log(msg.str(),ALOG_CONNECT);
}

// this is called when an async asio call encounters an error
void session::log_error(std::string msg,const boost::system::error_code& e) {
	std::stringstream err;
	
	err << "[Connection " << this << "] " << msg << " (" << e << ")";
	
	log(err.str(),LOG_ERROR);
}

// validates status codes that the end application is allowed to use
bool session::validate_app_close_status(uint16_t status) {
	if (status == CLOSE_STATUS_NORMAL) {
		return true;
	}
	
	if (status >= 4000 && status < 5000) {
		return true;
	}
	
	return false;
}

void session::drop_tcp(bool dropped_by_me) {
	m_timer.cancel();
	try {
		if (m_socket.is_open()) {
			m_socket.shutdown(tcp::socket::shutdown_both);
			m_socket.close();
		}
	} catch (boost::system::system_error& e) {
		if (e.code() == boost::asio::error::not_connected) {
			// this means the socket was disconnected by the other side before
			// we had a chance to. Ignore and continue.
		} else {
			throw e;
		}
	}
	m_dropped_by_me = dropped_by_me;
	m_state = STATE_CLOSED;
	
}
