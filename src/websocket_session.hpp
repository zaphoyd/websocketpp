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

/*
 
 Exit path mapping
 
 In every path:
 - If it is safe to close cleanly, close cleanly
 - Write to the access log on clean close
 - Write to the error log on unclean close and clean closes with a server error.
 - If session state is open and a local client is connected, send on_close msg
 
 - make sure the following bits are properly set:
 
 - If we initiated the close by sending the first close frame or by dropping the TCP connection, set closed_by_me. If the other endpoint sent the first close method or we got an EOF while reading clear closed_by_me
 - If we initiated the TCP connection drop set dropped_by_me. If we got EOF while reading clear dropped_by_me
 - If we sent and received a close frame or we received and sent an acknowledgement close frame set was_clean to true.
 
 - If we are the server we should drop TCP immediately
 - If we are the client we should drop TCP immediately except in the case where we just recieved an acknowledgement close frame. In this case wait a certain period of time for the server EOF.
 
 Questions:
 - if the client rejects
 
 Paths: (+ indicates path has been checked and implimented)
 Server Handshake Paths
 - Accept connection, read handshake, handshake is valid, write handshake, no errors. This is the correct path and leads to the frame reading paths
 - Accept connection, connection is not in state open after a time out (due to no bytes being read or no CRLFCRLF being read). This needs a time out after which we drop TCP.
 - Accept connection, read handshake, handshake is invalid. write HTTP error. drop TCP
 - Accept connection, read handshake, handshake is valid, write handshake returns EOF. This means client rejected something about our response. We should drop and notify our client. (note alternative client handshake reject method is to accept the handshake then immediately send a close message with the non-acceptance reason)
 - Accept connection, read handshake, handshake is valid, write handshake returns another error. We should drop and notify our client.
 Client Handshake Paths
 - 
 Server Frame Reading Paths
 - async read returns EOF. Close our own socket and notify our local interface.
 - async read returns another error
 
 
 
 
 Timeouts:
 - handshake timeout
 - wait for close frame after error
 - (client) wait for server to drop tcp after close handshake
 - idle client timeout? API specifiable?
 - wait for pong?
 
 */

#ifndef WEBSOCKET_SESSION_HPP
#define WEBSOCKET_SESSION_HPP

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#if defined(WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include <cstdlib>
#include <algorithm>
#include <exception>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace websocketpp {
	class session;
	typedef boost::shared_ptr<session> session_ptr;
	
	class handshake_error;
}

#include "websocketpp.hpp"
#include "websocket_frame.hpp"
#include "websocket_connection_handler.hpp"

#include "base64/base64.h"
#include "sha1/sha1.h"
#include "utf8_validator/utf8_validator.hpp"

using boost::asio::ip::tcp;

namespace websocketpp {

typedef std::map<std::string,std::string> header_list;

template <template <class rng_policy> endpoint_policy>
class session : public boost::enable_shared_from_this<session<endpoint_policy<rng_policy> > > {
public:
	typedef endpoint_policy<rng_policy> endpoint_t;
	typedef boost::shared_ptr<session<endpoint_t> > ptr;
	
	
	friend class handshake_error;
	
	namespace state {
		enum value {
			CONNECTING = 0,
			OPEN = 1,
			CLOSING = 2,
			CLOSED = 3
		};
	}

	session (endpoint_t::ptr e,
			 boost::asio::io_service& io_service,
			 connection_handler_ptr defc,
			 uint64_t buf_size)
	: m_state(STATE_CONNECTING),
	  m_writing(false),
	  m_local_close_code(close::status::NO_STATUS),
	  m_remote_close_code(close::status::NO_STATUS),
	  m_was_clean(false),
	  m_closed_by_me(false),
	  m_dropped_by_me(false),
	  m_socket(io_service),
	  m_io_service(io_service),
	  m_local_interface(defc),
	  m_timer(io_service,boost::posix_time::seconds(0)),
	  m_buf(buf_size), // maximum buffered (unconsumed) bytes from network
	  m_utf8_state(utf8_validator::UTF8_ACCEPT),
	  m_utf8_codepoint(0),
	  m_read_frame(e->get_rng()),
	  m_write_frame(e->get_rng())
	{
		
	}
	
	tcp::socket& socket() {
		return m_socket;
	}
	boost::asio::io_service& io_service() {
		return m_io_service;
	}
	
	/*** SERVER INTERFACE ***/
	
	// This function is called to begin the session loop. This method and all
	// that come after it are called as a result of an async event completing.
	// if any method in this chain returns before adding a new async event the
	// session will end.
	virtual void on_connect() = 0;
	
	// sets the internal connection handler of this connection to new_con.
	// This is useful if you want to switch handler objects during a connection
	// Example: a generic lobby handler could validate the handshake negotiate a
	// sub protocol to talk to and then pass the connection off to a handler for
	// that sub protocol.
	void set_handler(connection_handler_ptr new_con) {
		if (m_local_interface) {
			// TODO: this should be another method and not reusing onclose
			//m_local_interface->disconnect(shared_from_this(),4000,"Setting new connection handler");
		}
		m_local_interface = new_con;
		m_local_interface->on_open(shared_from_this());
	}
	
	
	/*** HANDSHAKE INTERFACE ***/
	// Set session connection information (avaliable only before/during the
	// opening handshake)
	
	// Get session status (valid once the connection is open)
	
	// returns the subprotocol that was negotiated during the opening handshake
	// or the empty string if no subprotocol was requested.
	const std::string& get_subprotocol() const {
		if (m_state == state::CONNECTING) {
			log("Subprotocol is not avaliable before the handshake has completed.",LOG_WARN);
			throw server_error("Subprotocol is not avaliable before the handshake has completed.");
		}
		return m_server_subprotocol;
	}
	
	const std::string& get_resource() const {
		return m_resource;
	}
	const std::string& get_origin() const {
		return m_client_origin;
	}
	std::string get_client_header(const std::string& key) const {
		return get_header(key,m_client_headers);
	}
	std::string get_server_header(const std::string& key) const {
		return get_header(key,m_server_headers);
	}
	const std::vector<std::string>& get_extensions() const {
		return m_server_extensions;
	}
	unsigned int get_version() const {
		return m_version;
	}
	
	/*** SESSION INTERFACE ***/
	
	// send basic frame types
	void send(const std::string &msg) {
		if (m_state != state::OPEN) {
			log("Tried to send a message from a session that wasn't open",LOG_WARN);
			return;
		}
		m_write_frame.set_fin(true);
		m_write_frame.set_opcode(frame::opcode::TEXT);
		m_write_frame.set_payload(msg);
		
		write_frame();
	}
	
	void send(const std::vector<unsigned char> &data) {
		if (m_state != state::OPEN) {
			log("Tried to send a message from a session that wasn't open",LOG_WARN);
			return;
		}
		m_write_frame.set_fin(true);
		m_write_frame.set_opcode(frame::opcode::BINARY);
		m_write_frame.set_payload(data);
		
		write_frame();
	}
	void ping(const std::string &msg) {
		if (m_state != state::OPEN) {
			log("Tried to send a ping from a session that wasn't open",LOG_WARN);
			return;
		}
		m_write_frame.set_fin(true);
		m_write_frame.set_opcode(frame::opcode::PING);
		m_write_frame.set_payload(msg);
		
		write_frame();
	}
	void pong(const std::string &msg) {
		if (m_state != state::OPEN) {
			log("Tried to send a pong from a session that wasn't open",LOG_WARN);
			return;
		}
		m_write_frame.set_fin(true);
		m_write_frame.set_opcode(frame::opcode::PONG);
		m_write_frame.set_payload(msg);
		
		write_frame();
	}
	
	// initiate a connection close
	void close(uint16_t status,const std::string &reason) {
		validate_app_close_status(status);
		send_close(status,msg);
	}

	virtual bool is_server() const = 0;

	// Opening handshake processors and callbacks. These need to be defined in
	// derived classes.
	virtual void handle_write_handshake(const boost::system::error_code& e) = 0;
	virtual void handle_read_handshake(const boost::system::error_code& e,
	                                   std::size_t bytes_transferred) = 0;
public: //protected:
	virtual void write_handshake() = 0;
	virtual void read_handshake() = 0;
	
	void read_frame() {
		// the initial read in the handshake may have read in the first frame.
		// handle it (if it exists) before we read anything else.
		handle_read_frame(boost::system::error_code());
	}
	// handle_read_frame reads and processes all socket read commands for the 
	// session by consuming the read buffer and then starting an async read with
	// itself as the callback. The connection is over when this method returns.
	void handle_read_frame (const boost::system::error_code& error) {
		if (m_state != state::OPEN && m_state != state::CLOSING) {
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
				m_state = state::CLOSED;
			}
		}
		
		std::istream s(&m_buf);
		
		while (m_buf.size() > 0 && m_state != state::CLOSED) {
			try {
				if (m_read_frame.get_bytes_needed() == 0) {
					throw frame_error("have bytes that no frame needs",frame::error::FATAL_SESSION_ERROR);
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
				
				if (m_read_frame.ready()) {
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
				if (m_read_frame.ready()) {
					m_read_frame.reset();
				}
				
				// process different types of frame errors
				// 
				if (e.code() == frame::FERR_PROTOCOL_VIOLATION) {
					send_close(close::status::PROTOCOL_ERROR, e.what());
				} else if (e.code() == frame::FERR_PAYLOAD_VIOLATION) {
					send_close(close::status::INVALID_PAYLOAD, e.what());
				} else if (e.code() == frame::FERR_INTERNAL_SERVER_ERROR) {
					send_close(close::status::ABNORMAL_CLOSE, e.what());
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
			m_state = state::CLOSED;
		}
		
		// we have read everything, check if we should read more
		
		if ((m_state == state::OPEN || m_state == state::CLOSING) && m_read_frame.get_bytes_needed() > 0) {
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
		} else if (m_state == state::CLOSED) {
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
	
	// write m_write_frame out to the socket.
	void write_frame();
	void handle_write_frame (const boost::system::error_code& error) {
		if (error) {
			log_error("Error writing frame data",error);
			drop_tcp(false);
		}
		
		access_log("handle_write_frame complete",ALOG_FRAME);
		m_writing = false;
	}
	
	void handle_timer_expired(const boost::system::error_code& error) {
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
	void handle_handshake_expired(const boost::system::error_code& error) {
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
	void handle_close_expired(const boost::system::error_code& error) {
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
	// The error timer is set when we want to give the other endpoint some time to
	// do something but don't want to wait forever. There is a special error code
	// that represents the timer being canceled by us (because the other endpoint
	// responded in time. All other cases should assume that the other endpoint is
	// irrepairibly broken and drop the TCP connection.
	void handle_error_timer_expired (const boost::system::error_code& error) {
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
	
	// helper functions for processing each opcode
	void process_frame() {
		log("process_frame",LOG_DEBUG);
		
		if (m_state == state::OPEN) {
			switch (m_read_frame.get_opcode()) {
				case frame::opcode::CONTINUATION:
					process_continuation();
					break;
				case frame::opcode::TEXT:
					process_text();
					break;
				case frame::opcode::BINARY:
					process_binary();
					break;
				case frame::opcode::CLOSE:
					log("process_close",LOG_DEBUG);
					process_close();
					break;
				case frame::opcode::PING:
					process_ping();
					break;
				case frame::opcode::PONG:
					process_pong();
					break;
				default:
					throw frame::exception("Invalid Opcode",
									  frame::FERR_PROTOCOL_VIOLATION);
					break;
			}
		} else if (m_state == state::CLOSING) {
			if (m_read_frame.get_opcode() == frame::opcode::CLOSE) {
				process_close();
			} else {
				// Ignore all other frames in closing state
				log("ignoring this frame",LOG_DEBUG);
			}
		} else {
			// Recieved message before or after connection was opened/closed
			throw frame::exception("process_frame called from invalid state");
		}
		
		m_read_frame.reset();
	}
	void process_ping() {
		access_log("Ping",ALOG_MISC_CONTROL);
		// TODO: on_ping
		
		// send pong
		m_write_frame.set_fin(true);
		m_write_frame.set_opcode(frame::PONG);
		m_write_frame.set_payload(m_read_frame.get_payload());
		
		write_frame();
	}
	void process_pong() {
		access_log("Pong",ALOG_MISC_CONTROL);
		// TODO: on_pong
	}
	void process_text() {
		// this will throw an exception if validation fails at any point
		m_read_frame.validate_utf8(&m_utf8_state,&m_utf8_codepoint);
		
		// otherwise, treat as binary
		process_binary();
	}
	void process_binary() {
		if (m_fragmented) {
			throw frame::exception("Got a new message before the previous was finished.",frame::FERR_PROTOCOL_VIOLATION);
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
	void process_continuation() {
		if (!m_fragmented) {
			throw frame::exception("Got a continuation frame without an outstanding message.",frame::FERR_PROTOCOL_VIOLATION);
		}
		
		if (m_current_opcode == frame::opcode::TEXT) {
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
	void process_close() {
		m_remote_close_code = m_read_frame.get_close_status();
		m_remote_close_msg = m_read_frame.get_close_msg();
		
		if (m_state == state::OPEN) {
			log("process_close sending ack",LOG_DEBUG);
			// This is the case where the remote initiated the close.
			m_closed_by_me = false;
			// send acknowledgement
			
			// TODO: check if the remote close code
			if (m_remote_close_code >= close::status::RSV_START) {
				
			}
			
			send_close(m_remote_close_code,m_remote_close_msg);
		} else if (m_state == state::CLOSING) {
			log("process_close got ack",LOG_DEBUG);
			// this is an ack of our close message
			m_closed_by_me = true;
		} else {
			throw frame::exception("process_closed called from wrong state");
		}
		
		m_was_clean = true;
		m_state = state::CLOSED;
	}
	
	// deliver message if we have a local interface attached
	void deliver_message() {
		if (!m_local_interface) {
			return;
		}
		
		if (m_current_opcode == frame::opcode::BINARY) {
			//log("Dispatching Binary Message",LOG_DEBUG);
			if (m_fragmented) {
				m_local_interface->on_message(shared_from_this(),m_current_message);
			} else {
				m_local_interface->on_message(shared_from_this(),
											  m_read_frame.get_payload());
			}
		} else if (m_current_opcode == frame::opcode::TEXT) {
			std::string msg;
			
			// make sure the finished frame is valid utf8
			// the streaming validator checks for bad codepoints as it goes. It 
			// doesn't know where the end of the message is though, so we need to 
			// check here to make sure the final message ends on a valid codepoint.
			if (m_utf8_state != utf8_validator::UTF8_ACCEPT) {
				throw frame::exception("Invalid UTF-8 Data",
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
			throw frame::exception(err.str(),frame::error::SOFT_SESSION_ERROR);
		}
	}
	
	// copies the current read frame payload into the session so that the read
	// frame can be cleared for the next read. This is done when fragmented
	// messages are recieved.
	void extract_payload() {
		std::vector<unsigned char> &msg = m_read_frame.get_payload();
		m_current_message.resize(m_current_message.size()+msg.size());
		std::copy(msg.begin(),msg.end(),m_current_message.end()-msg.size());
	}
	
	// reset session for a new message
	void reset_message() {
		m_error = false;
		m_fragmented = false;
		m_current_message.clear();
		
		m_utf8_state = utf8_validator::UTF8_ACCEPT;
		m_utf8_codepoint = 0;
	}
	
	// logging
	virtual void log(const std::string& msg, uint16_t level) const = 0;
	virtual void access_log(const std::string& msg, uint16_t level) const = 0;
	
	void log_close_result() {
		std::stringstream msg;
		
		msg << "[Connection " << this << "] "
		<< (m_was_clean ? "Clean " : "Unclean ")
		<< "close local:[" << m_local_close_code
		<< (m_local_close_msg == "" ? "" : ","+m_local_close_msg) 
		<< "] remote:[" << m_remote_close_code 
		<< (m_remote_close_msg == "" ? "" : ","+m_remote_close_msg) << "]";
		
		access_log(msg.str(),ALOG_DISCONNECT);
	}
	void log_open_result() {
		std::stringstream msg;
		
		msg << "[Connection " << this << "] "
	    << m_socket.remote_endpoint()
	    << " v" << m_version << " "
	    << (get_client_header("User-Agent") == "" ? "NULL" : get_client_header("User-Agent")) 
	    << " " << m_resource << " " << m_server_http_code;
		
		access_log(msg.str(),ALOG_HANDSHAKE);
	}
	// this is called when an async asio call encounters an error
	void log_error(std::string msg,const boost::system::error_code& e) {
		std::stringstream err;
		
		err << "[Connection " << this << "] " << msg << " (" << e << ")";
		
		log(err.str(),LOG_ERROR);
	}
	
	// misc helpers
	
	// validates status codes that the end application is allowed to use
	bool validate_app_close_status(close::status::value status) {
		if (status == close::status::NORMAL) {
			return true;
		}
		
		if (status >= 4000 && status < 5000) {
			return true;
		}
		
		return false;
	}
	void send_close(close::status::value status,const std::string& reason) {
		if (m_state != state::OPEN) {
			log("Tried to disconnect a session that wasn't open",LOG_WARN);
			return;
		}
		
		m_state = state::CLOSING;
		
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
		m_write_frame.set_opcode(frame::opcode::CLOSE);
		
		// echo close value unless there is a good reason not to.
		if (status == close::status::NO_STATUS) {
			m_write_frame.set_status(close::status::NORMAL,"");
		} else if (status == close::status::ABNORMAL_CLOSE) {
			// Internal implimentation error. There is no good close code for this.
			m_write_frame.set_status(close::status::POLICY_VIOLATION,message);
		} else if (close::status::invalid(status)) {
			m_write_frame.set_status(close::status::PROTOCOL_ERROR,"Status code is invalid");
		} else if (close::status::reserved(status)) {
			m_write_frame.set_status(close::status::PROTOCOL_ERROR,"Status code is reserved");
		} else {
			m_write_frame.set_status(status,message);
		}
		
		write_frame() {
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
	}
	void drop_tcp(bool dropped_by_me = true) {
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
		m_state = state::CLOSED;
	}
private:
	std::string get_header(const std::string& key,
	                       const header_list& list) const {
		header_list::const_iterator h = list.find(key);
		
		if (h == list.end()) {
			return "";
		} else {
			return h->second;
		}
	}

protected:
	// Immutable state about the current connection from the handshake
	// Client handshake
	std::string					m_raw_client_handshake;
	std::string					m_client_http_request;
	std::string					m_resource;
	std::string					m_client_origin;
	header_list					m_client_headers;
	std::vector<std::string>	m_client_subprotocols;
	std::vector<std::string>	m_client_extensions;
	unsigned int				m_version;

	// Server handshake
	std::string					m_raw_server_handshake;
	std::string					m_server_http_request;
	header_list					m_server_headers;
	std::string					m_server_subprotocol;
	std::vector<std::string>	m_server_extensions;
	uint16_t					m_server_http_code;
	std::string					m_server_http_string;

	// Mutable connection state;
	uint8_t						m_state;
	bool						m_writing;

	// Close state
	close::status::value		m_local_close_code;
	std::string					m_local_close_msg;
	close::status::value		m_remote_close_code;
	std::string					m_remote_close_msg;
	bool						m_was_clean;
	bool						m_closed_by_me;
	bool						m_dropped_by_me;

	// Connection Resources
	tcp::socket 				m_socket;
	boost::asio::io_service&	m_io_service;
	connection_handler_ptr		m_local_interface;
	boost::asio::deadline_timer	m_timer;
	
	// Buffers
	boost::asio::streambuf		m_buf;
	
	// current message state
	uint32_t					m_utf8_state;
	uint32_t					m_utf8_codepoint;
	std::vector<unsigned char>	m_current_message;
	bool 						m_fragmented;
	frame::opcode::value 		m_current_opcode;
	
	// frame parsers
	frame::parser<rng_policy>	m_read_frame;
	frame::parser<rng_policy>	m_write_frame;
	
	// unknown
	bool						m_error;
};

// Exception classes

class handshake_error : public std::exception {
public:	
	handshake_error(const std::string& msg,
					int http_error,
					const std::string& http_msg = "")
		: m_msg(msg),m_http_error_code(http_error),m_http_error_msg(http_msg) {}
	~handshake_error() throw() {}
	
	virtual const char* what() const throw() {
		return m_msg.c_str();
	}
	
	std::string m_msg;
	int			m_http_error_code;
	std::string m_http_error_msg;
};

}

#endif // WEBSOCKET_SESSION_HPP
