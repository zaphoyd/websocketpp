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

#ifndef WEBSOCKET_SERVER_HPP
#define WEBSOCKET_SERVER_HPP

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/program_options.hpp>

//#include <boost/asio/ssl.hpp> // for the ssl policy only

namespace po = boost::program_options;

#include <set>
#include <queue>

#include "websocketpp.hpp"

#include "interfaces/session.hpp"

// Session processors
#include "interfaces/protocol.hpp"
#include "hybi_legacy_processor.hpp"
#include "hybi_processor.hpp"

#include "websocket_connection_handler.hpp"

#include "rng/blank_rng.hpp"
#include "http/parser.hpp"
#include "logger/logger.hpp"

using boost::asio::ip::tcp;
using websocketpp::session::server_handler_ptr;
using websocketpp::protocol::processor_ptr;

namespace websocketpp {
namespace server {

namespace write_state {
	enum value {
		IDLE = 0,
		WRITING = 1,
		INTURRUPT = 2
	};
}
	
template 
	<
	typename server_policy,
	template <class> class security_policy
	>
class connection 
	: 
	public security_policy< connection<server_policy,security_policy> >, 
	public boost::enable_shared_from_this< connection<server_policy,security_policy> >  {
public:
	typedef server_policy server_type;
	typedef connection<server_policy,security_policy> connection_type;
	typedef security_policy< connection<server_policy,security_policy> > security_policy_type;
		
	typedef boost::shared_ptr<connection_type> ptr;
	typedef boost::shared_ptr<server_type> server_ptr;
	
	connection(server_ptr s,
			boost::asio::io_service& io_service,
			server_handler_ptr handler)
	: security_policy_type(io_service),
	  m_server(s),
	  m_io_service(io_service),
	  //m_socket(io_service),
	  m_timer(io_service,boost::posix_time::seconds(0)),
	  m_buf(/* TODO: needs a max here */),
	  m_handler(handler),
	  m_state(session::state::CONNECTING),
	  m_write_buffer(0),
	  m_write_state(write_state::IDLE) {}
	
	// implimentation of the server session API
	
	// Valid always
	session::state::value get_state() const {
		// TODO: syncronize
		return m_state;
	}
	
	unsigned int get_version() const {
		return m_version;
	}
	
	std::string get_origin() const {
		return m_origin;
	}
	
	std::string get_request_header(const std::string& key) const {
		return m_request.header(key);
	}
	
	bool get_secure() const {
		// TODO
		return false;
	}
	
	std::string get_host() const {
		return m_uri.host;
	}
	
	uint16_t get_port() const {
		return m_uri.port;
	}
	
	std::string get_resource() const {
		return m_uri.resource;
	}
	
	tcp::endpoint get_endpoint() const {
		return security_policy_type::socket().remote_endpoint();
	}
	
	// Valid for CONNECTING state
	void add_response_header(const std::string& key, const std::string& value) {
		m_response.add_header(key,value);
	}
	void replace_response_header(const std::string& key, const std::string& value) {
		m_response.replace_header(key,value);
	}
	const std::vector<std::string>& get_subprotocols() const {
		return m_requested_subprotocols;
	}
	const std::vector<std::string>& get_extensions() const {
		return m_requested_extensions;
	}
	void select_subprotocol(const std::string& value) {
		std::vector<std::string>::iterator it;
		
		it = std::find(m_requested_subprotocols.begin(),
					   m_requested_subprotocols.end(),
					   value);
		
		if (value != "" && it == m_requested_subprotocols.end()) {
			throw server_error("Attempted to choose a subprotocol not proposed by the client");
		}
		
		m_subprotocol = value;
	}
	void select_extension(const std::string& value) {
		if (value == "") {
			return;
		}
		
		std::vector<std::string>::iterator it;
		
		it = std::find(m_requested_extensions.begin(),
					   m_requested_extensions.end(),
					   value);
		
		if (it == m_requested_extensions.end()) {
			throw server_error("Attempted to choose an extension not proposed by the client");
		}
		
		m_extensions.push_back(value);
	}
	
	// Valid for OPEN state
	
	// These functions invoke write_message through the io_service to gain 
	// thread safety
	void send(const utf8_string& payload) {
		binary_string_ptr msg(m_processor->prepare_frame(frame::opcode::TEXT,false,payload));
		
		m_io_service.post(boost::bind(&connection_type::write_message,connection_type::shared_from_this(),msg));
		
		// TODO: return bytes in flight somehow?
	}
	
	void send(const binary_string& data) {
		binary_string_ptr msg(m_processor->prepare_frame(frame::opcode::BINARY,false,data));
		m_io_service.post(boost::bind(&connection_type::write_message,connection_type::shared_from_this(),msg));
	}
	
	void close(close::status::value code, const utf8_string& reason) {
		// TODO
	}
	
	void ping(const binary_string& payload) {
		binary_string_ptr msg(m_processor->prepare_frame(frame::opcode::PING,false,payload));
		
		m_io_service.post(boost::bind(&connection_type::write_message,connection_type::shared_from_this(),msg));
	}
	
	void pong(const binary_string& payload) {
		binary_string_ptr msg(m_processor->prepare_frame(frame::opcode::PONG,false,payload));
		m_io_service.post(boost::bind(&connection_type::write_message,connection_type::shared_from_this(),msg));
	}
	
	uint64_t buffered_amount() const {
		// TODO: syncronize this member function
		return m_write_buffer;
	}
	
	// Valid for CLOSED state
	close::status::value get_local_close_code() const {
		return m_local_close_code;
	}
	utf8_string get_local_close_reason() const {
		return m_local_close_reason;
	}
	close::status::value get_remote_close_code() const {
		return m_remote_close_code;
	}
	utf8_string get_remote_close_reason() const {
		return m_remote_close_reason;
	}
	bool get_failed_by_me() const {
		return m_failed_by_me;
	}
	bool get_dropped_by_me() const {
		return m_dropped_by_me;
	}
	bool get_closed_by_me() const {
		return m_closed_by_me;
	}
	
	////////
	
	// now provided by a policy class
	/*tcp::socket& get_socket() {
		return m_socket;
	}*/
	
	void read_request() {
		// start reading HTTP header and attempt to determine if the incoming 
		// connection is a websocket connection. If it is determine the version
		// and generate a session processor for that version. If it is not a 
		// websocket connection either drop or pass to the default HTTP pass 
		// through handler.
		
		m_timer.expires_from_now(boost::posix_time::seconds(5 /* TODO */));
		
		m_timer.async_wait(
		    boost::bind(
		        &connection_type::fail_on_expire,
		        connection_type::shared_from_this(),
		        boost::asio::placeholders::error
		    )
		);
		
		boost::asio::async_read_until(
		    security_policy_type::socket(),
		    m_buf,
		    "\r\n\r\n",
		    boost::bind(
		        &connection_type::handle_read_request,
		        connection_type::shared_from_this(),
		        boost::asio::placeholders::error,
		        boost::asio::placeholders::bytes_transferred
		    )
		);
	}
	
	void handle_read_request(const boost::system::error_code& e,
							 std::size_t bytes_transferred) {
		if (e) {
			log_error("Error reading HTTP request",e);
			terminate_connection(false);
			return;
		}
		
		try {
			std::istream request(&m_buf);
			
			// TODO: use a more generic consume api where we just call read_some
			// and have the handshake consume and validate as we go.
			// 
			// For now, because it simplifies things we will use the parse_header
			// member function which requires the complete header to be passed in
			// initially. ASIO can guarantee us this.
			//
			// 
			//m_remote_handshake.consume(response_stream);
			if (!m_request.parse_complete(request)) {
				// not a valid HTTP request/response
				throw http::exception("Recieved invalid HTTP Request",http::status_code::BAD_REQUEST);
			}
			
			// Log the raw handshake.
			m_server->alog().at(log::alevel::DEBUG_HANDSHAKE) << m_request.raw() << log::endl;
			
			// Determine what sort of connection this is:
			m_version = -1;
			
			std::string h = m_request.header("Upgrade");
			if (boost::ifind_first(h,"websocket")) {
				h = m_request.header("Sec-WebSocket-Version");
				if (h == "") {
					m_version = 0;
				} else {
					m_version = atoi(h.c_str());
					if (m_version == 0) {
						throw(http::exception("Unable to determine connection version",http::status_code::BAD_REQUEST));
					}
				}
			}
			
			m_server->alog().at(log::alevel::DEBUG_HANDSHAKE) << "determined connection version: " << m_version << log::endl;
			
			if (m_version == -1) {
				// Probably a plain HTTP request
				// TODO: forward to an http handler?
				
			} else {
				// websocket connection
				// create a processor based on version.
				if (m_version == 0) {
					// create hybi 00 processor
					
					// grab hybi00 token first
					char foo[9];
					foo[8] = 0;
					
					request.get(foo,9);
					
					if (request.gcount() != 8) {
						throw http::exception("Missing Key3",http::status_code::BAD_REQUEST);
					}
					m_request.add_header("Sec-WebSocket-Key3",std::string(foo));
					
					m_processor = processor_ptr(new protocol::hybi_legacy_processor(false));
				} else if (m_version == 7 || m_version == 8 || m_version == 13) {
					// create hybi 17 processor
					m_processor = processor_ptr(new protocol::hybi_processor<blank_rng>(false,m_rng));
				} else {
					// TODO: respond with unknown version message per spec
				}
				
				// ask new protocol whether this set of headers is valid
				m_processor->validate_handshake(m_request);
				m_origin = m_processor->get_origin(m_request);
				m_uri = m_processor->get_uri(m_request);
				
				// ask local application to confirm that it wants to accept
				m_handler->validate(boost::static_pointer_cast<websocketpp::session::server>(connection_type::shared_from_this()));
				
				m_response.set_status(http::status_code::SWITCHING_PROTOCOLS);
			}
			
		} catch (const http::exception& e) {
			m_server->alog().at(log::alevel::DEBUG_HANDSHAKE) << e.what() << log::endl;
			
			m_server->elog().at(log::elevel::ERROR) 
			<< "Caught handshake exception: " << e.what() << log::endl;
			
			m_response.set_status(e.m_error_code,e.m_error_msg);
		}
		
		write_response();
	}
	
	// write the response to the client's request.
	void write_response() {
		std::string response;
		
		m_response.set_version("HTTP/1.1");
		
		if (m_response.status_code() == http::status_code::SWITCHING_PROTOCOLS) {
			// websocket response
			m_processor->handshake_response(m_request,m_response);
			
			if (m_subprotocol != "") {
				m_response.replace_header("Sec-WebSocket-Protocol",m_subprotocol);
			}
			
			// TODO: return negotiated extensions
		} else {
			// HTTP response
		}
		
		m_response.replace_header("Server","WebSocket++/2011-10-31");
		
		std::string raw = m_response.raw();
		
		// Hack for legacy HyBi
		if (m_version == 0) {
			raw += boost::dynamic_pointer_cast<protocol::hybi_legacy_processor>(m_processor)->get_key3();
		}
		
		m_server->alog().at(log::alevel::DEBUG_HANDSHAKE) << raw << log::endl;
		
		// start async write to handle_write_handshake
		boost::asio::async_write(
		    security_policy_type::socket(),
		    boost::asio::buffer(raw),
		    boost::bind(
		        &connection_type::handle_write_response,
		        connection_type::shared_from_this(),
		        boost::asio::placeholders::error
		    )
		);
	}
	
	void handle_write_response(const boost::system::error_code& error) {
		// stop the handshake timer
		m_timer.cancel();
		
		if (error) {
			log_error("Network error writing handshake response",error);
			terminate_connection(false);
			m_handler->on_fail(boost::static_pointer_cast<websocketpp::session::server>(connection_type::shared_from_this()));
			return;
		}
		
		log_open_result();
		
		// log error if this was 
		if (m_version != -1 && m_response.status_code() != http::status_code::SWITCHING_PROTOCOLS) {
			m_server->elog().at(log::elevel::ERROR) 
			<< "Handshake ended with HTTP error: " 
			<< m_response.status_code() << " " << m_response.status_msg() 
			<< log::endl;
			
			terminate_connection(true);
			m_handler->on_fail(boost::static_pointer_cast<websocketpp::session::server>(connection_type::shared_from_this()));
			return;
		}
		
		m_state = session::state::OPEN;
		
		m_handler->on_open(boost::static_pointer_cast<websocketpp::session::server>(connection_type::shared_from_this()));
		
		// TODO: start read message loop.
		m_server->alog().at(log::alevel::DEVEL) << "calling handle_read_frame" << log::endl;
		handle_read_frame(boost::system::error_code());
	}
	
	void handle_read_frame (const boost::system::error_code& error) {
		// check if state changed while we were waiting for a read.
		if (m_state == session::state::CLOSED) { return; }
		
		if (error) {
			if (error == boost::asio::error::eof) {			
				// got unexpected EOF
				// TODO: log error
				terminate_connection(false);
			} else if (error == boost::asio::error::operation_aborted) {
				// got unexpected abort (likely our server issued an abort on
				// all connections on this io_service)
				
				// TODO: log error
				terminate_connection(true);
			} else {
				// Other unexpected error
				
				// TODO: log error
				terminate_connection(false);
			}
		}
				
		// process data from the buffer just read into
		std::istream s(&m_buf);
		
		m_server->alog().at(log::alevel::DEVEL) << "starting while, buffer size: " << m_buf.size() << log::endl;
		
		while (m_state != session::state::CLOSED && m_buf.size() > 0) {
			try {
				m_server->alog().at(log::alevel::DEVEL) << "starting consume, buffer size: " << m_buf.size() << log::endl;
				m_processor->consume(s);
				m_server->alog().at(log::alevel::DEVEL) << "done consume, buffer size: " << m_buf.size() << log::endl;
				
				if (m_processor->ready()) {
					m_server->alog().at(log::alevel::DEVEL) << "new message ready" << m_buf.size() << log::endl;
					
					bool response;
					switch (m_processor->get_opcode()) {
						case frame::opcode::TEXT:
							m_handler->on_message(boost::static_pointer_cast<websocketpp::session::server>(connection_type::shared_from_this()),m_processor->get_utf8_payload());
							break;
						case frame::opcode::BINARY:
							m_handler->on_message(boost::static_pointer_cast<websocketpp::session::server>(connection_type::shared_from_this()),m_processor->get_binary_payload());
							break;
						case frame::opcode::PING:
							response = m_handler->on_ping(boost::static_pointer_cast<websocketpp::session::server>(connection_type::shared_from_this()),m_processor->get_binary_payload());
							
							if (response) {
								// send response ping
								write_message(m_processor->prepare_frame(frame::opcode::PONG,false,*m_processor->get_binary_payload()));
							}
							break;
						case frame::opcode::PONG:
							m_handler->on_pong(boost::static_pointer_cast<websocketpp::session::server>(connection_type::shared_from_this()),m_processor->get_binary_payload());
							
							// TODO: disable ping response timer
							
							break;
						case frame::opcode::CLOSE:
							m_remote_close_code = m_processor->get_close_code();
							m_remote_close_reason = m_processor->get_close_reason();
							
							// check that the codes we got over the wire are
							// valid
							
							if (close::status::invalid(m_remote_close_code)) {
								throw session::exception("Invalid close code",session::error::PROTOCOL_VIOLATION);
							}
							
							if (close::status::reserved(m_remote_close_code)) {
								throw session::exception("Reserved close code",session::error::PROTOCOL_VIOLATION);
							}
							
							if (m_state == session::state::OPEN) {
								// other end is initiating
								m_server->elog().at(log::elevel::DEVEL) 
								<< "sending close ack" << log::endl;
																
								send_close_ack();
							} else if (m_state == session::state::CLOSING) {
								// ack of our close
								m_server->elog().at(log::elevel::DEVEL) 
								<< "got close ack" << log::endl;
								
								terminate_connection(false);
								// TODO: start terminate timer (if client)
							}
							break;
						default:
							throw session::exception("Invalid Opcode",session::error::PROTOCOL_VIOLATION);
							break;
					}
					m_processor->reset();
				}
			} catch (const session::exception& e) {
				m_server->elog().at(log::elevel::ERROR) 
				    << "Caught session exception: " << e.what() << log::endl;
				
				// if the exception happened while processing.
				// TODO: this is not elegant, perhaps separate frame read vs process
				// exceptions need to be used.
				if (m_processor->ready()) {
					m_processor->reset();
				}
				
				if (e.code() == session::error::PROTOCOL_VIOLATION) {
					send_close(close::status::PROTOCOL_ERROR, e.what());
				} else if (e.code() == session::error::PAYLOAD_VIOLATION) {
					send_close(close::status::INVALID_PAYLOAD, e.what());
				} else if (e.code() == session::error::INTERNAL_SERVER_ERROR) {
					send_close(close::status::POLICY_VIOLATION, e.what());
				} else if (e.code() == session::error::SOFT_ERROR) {
					// ignore and continue processing frames
					continue;
				} else {
					// Fatal error, forcibly end connection immediately.
					m_server->elog().at(log::elevel::DEVEL) 
					<< "Dropping TCP due to unrecoverable exception" 
					<< log::endl;
					terminate_connection(true);
				}
				break;
			}
		}
		
		// try and read more
		if (m_state != session::state::CLOSED && 
			m_processor->get_bytes_needed() > 0) {
			// TODO: read timeout timer?
			
			boost::asio::async_read(
				security_policy_type::socket(),
				m_buf,
				boost::asio::transfer_at_least(m_processor->get_bytes_needed()),
				boost::bind(
					&connection_type::handle_read_frame,
					connection_type::shared_from_this(),
					boost::asio::placeholders::error
				)
			);
		}
	}
	
	void send_close(close::status::value code, const std::string& reason) {
		if (m_state != session::state::OPEN) {
			m_server->elog().at(log::elevel::WARN) 
			<< "Tried to disconnect a session that wasn't open" << log::endl;
			return;
		}
		
		if (close::status::invalid(code)) {
			m_server->elog().at(log::elevel::WARN) 
			<< "Tried to close a connection with invalid close code: " << code << log::endl;
			return;
		} else if (close::status::reserved(code)) {
			m_server->elog().at(log::elevel::WARN) 
			<< "Tried to close a connection with reserved close code: " << code << log::endl;
			return;
		}
		
		m_state = session::state::CLOSING;
		
		m_closed_by_me = true;
		
		m_timer.expires_from_now(boost::posix_time::milliseconds(1000));
		m_timer.async_wait(
			boost::bind(
				&connection_type::fail_on_expire,
				connection_type::shared_from_this(),
				boost::asio::placeholders::error
			)
		);
		
		m_local_close_code = code;
		m_local_close_reason = reason;
		
		
		write_message(m_processor->prepare_close_frame(m_local_close_code,
													   false,
													   m_local_close_reason));
		m_write_state = write_state::INTURRUPT;
	}
	
	// send an acknowledgement close frame
	void send_close_ack() {
		// TODO: state should be OPEN
		
		// echo close value unless there is a good reason not to.
		if (m_remote_close_code == close::status::NO_STATUS) {
			m_local_close_code = close::status::NORMAL;
			m_local_close_reason = "";
		} else if (m_remote_close_code == close::status::ABNORMAL_CLOSE) {
			// TODO: can we possibly get here? This means send_close_ack was
			//       called after a connection ended without getting a close
			//       frame
			throw "shouldn't be here";
		} else if (close::status::invalid(m_remote_close_code)) {
			m_local_close_code = close::status::PROTOCOL_ERROR;
			m_local_close_reason = "Status code is invalid";
		} else if (close::status::reserved(m_remote_close_code)) {
			m_local_close_code = close::status::PROTOCOL_ERROR;
			m_local_close_reason = "Status code is reserved";
		} else {
			m_local_close_code = m_remote_close_code;
			m_local_close_reason = m_remote_close_reason;
		}
		
		// TODO: check whether we should cancel the current in flight write.
		//       if not canceled the close message will be sent as soon as the
		//       current write completes.
		
		
		write_message(m_processor->prepare_close_frame(m_local_close_code,
													   false,
													   m_local_close_reason));
		m_write_state = write_state::INTURRUPT;
	}
	
	void write_message(binary_string_ptr msg) {
		m_write_buffer += msg->size();
		m_write_queue.push(msg);
		write();
	}
	
	void write() {
		switch (m_write_state) {
			case write_state::IDLE:
				break;
			case write_state::WRITING:
				// already writing. write() will get called again by the write
				// handler once it is ready.
				return;
			case write_state::INTURRUPT:
				// clear the queue except for the last message
				while (m_write_queue.size() > 1) {
					m_write_buffer -= m_write_queue.front()->size();
					m_write_queue.pop();
				}
				break;
			default:
				// TODO: assert shouldn't be here
				break;
		}
		
		if (m_write_queue.size() > 0) {
			if (m_write_state == write_state::IDLE) {
				m_write_state = write_state::WRITING;
			}
						
			boost::asio::async_write(
			    security_policy_type::socket(),
			    boost::asio::buffer(*m_write_queue.front()),
			    boost::bind(
			        &connection_type::handle_write,
			        connection_type::shared_from_this(),
			        boost::asio::placeholders::error
			    )
			);
		} else {
			// if we are in an inturrupted state and had nothing else to write
			// it is safe to terminate the connection.
			if (m_write_state == write_state::INTURRUPT) {
				terminate_connection(false);
			}
		}
	}
	
	void handle_write(const boost::system::error_code& error) {
		if (error) {
			if (error == boost::asio::error::operation_aborted) {
				// previous write was aborted
				std::cout << "aborted" << std::endl;
			} else {
				log_error("Error writing frame data",error);
				terminate_connection(false);
				return;
			}
		}
		
		if (m_write_queue.size() == 0) {
			std::cout << "handle_write called with empty queue" << std::endl;
			return;
		}
		
		m_write_buffer -= m_write_queue.front()->size();
		m_write_queue.pop();
		
		if (m_write_state == write_state::WRITING) {
			m_write_state = write_state::IDLE;
		}
		
		write();
	}
	
	// end conditions
	// - tcp connection is closed
	// - session state is CLOSED
	// - session end flags are set
	// - application is notified
	void terminate_connection(bool failed_by_me) {
		m_server->alog().at(log::alevel::DEBUG_CLOSE) << "terminate called" << log::endl;
		
		if (m_state == session::state::CLOSED) {
			// shouldn't be here
		}
		
		// cancel the close timeout
		m_timer.cancel();
		
		try {
			if (security_policy_type::socket().is_open()) {
				security_policy_type::socket().shutdown(tcp::socket::shutdown_both);
				security_policy_type::socket().close();
				m_dropped_by_me = true;
			}
		} catch (boost::system::system_error& e) {
			if (e.code() == boost::asio::error::not_connected) {
				// this means the socket was disconnected by the other side before
				// we had a chance to. Ignore and continue.
			} else {
				throw e;
			}
		}
		
		m_failed_by_me = failed_by_me;
		
		session::state::value old_state = m_state;
		m_state = session::state::CLOSED;
		
		// If we called terminate from the connecting state call on_fail
		if (old_state == session::state::CONNECTING) {
			m_handler->on_fail(boost::static_pointer_cast<websocketpp::session::server>(connection_type::shared_from_this()));
		} else if (old_state == session::state::OPEN || 
				   old_state == session::state::CLOSING) {
			m_handler->on_close(boost::static_pointer_cast<websocketpp::session::server>(connection_type::shared_from_this()));
		} else {
			// if we were already closed something is wrong
		}
	}
	
	// this is called when an async asio call encounters an error
	void log_error(std::string msg,const boost::system::error_code& e) {
		m_server->elog().at(log::elevel::ERROR) 
		<< msg << "(" << e << ")" << log::endl;
	}
	void log_close_result() {
		m_server->alog().at(log::alevel::DISCONNECT) 
		//<< "Disconnect " << (m_was_clean ? "Clean" : "Unclean")
		<< "Disconnect " 
		<< " close local:[" << m_local_close_code
		<< (m_local_close_reason == "" ? "" : ","+m_local_close_reason) 
		<< "] remote:[" << m_remote_close_code 
		<< (m_remote_close_reason == "" ? "" : ","+m_remote_close_reason) << "]"
		<< log::endl;
	}
	void log_open_result() {
		m_server->alog().at(log::alevel::CONNECT) << "Connection "
		<< security_policy_type::socket().remote_endpoint() << " v" << m_version << " "
		<< (get_request_header("User-Agent") == "" ? "NULL" : get_request_header("User-Agent")) 
		<< " " << m_uri.resource << " " << m_response.status_code() 
		<< log::endl;
	}
	
	void fail_on_expire(const boost::system::error_code& error) {
		if (error) {
			if (error != boost::asio::error::operation_aborted) {
				m_server->elog().at(log::elevel::DEVEL) 
				<< "fail_on_expire timer ended in unknown error" << log::endl;
				terminate_connection(false);
			}
			return;
		}
		m_server->elog().at(log::elevel::DEVEL) 
		<< "fail_on_expire timer expired" << log::endl;
		terminate_connection(true);
	}
	
private:
	server_ptr					m_server;
	
	boost::asio::io_service&	m_io_service;
	//tcp::socket					m_socket;
	boost::asio::deadline_timer	m_timer;
	boost::asio::streambuf		m_buf;
	
	server_handler_ptr			m_handler;
	processor_ptr				m_processor;
	blank_rng					m_rng;
	
	// Connection state
	http::parser::request		m_request;
	http::parser::response		m_response;
	
	std::vector<std::string>	m_requested_subprotocols;
	std::vector<std::string>	m_requested_extensions;
	std::string					m_subprotocol;
	std::vector<std::string>	m_extensions;
	std::string					m_origin;
	unsigned int				m_version;
	bool						m_secure;
	ws_uri						m_uri;
	
	session::state::value		m_state;
	
	// Write queue
	std::queue<binary_string_ptr>	m_write_queue;
	uint64_t						m_write_buffer;
	write_state::value				m_write_state;
	
	// Close state
	close::status::value		m_local_close_code;
	std::string					m_local_close_reason;
	close::status::value		m_remote_close_code;
	std::string					m_remote_close_reason;
	bool						m_closed_by_me;
	bool						m_failed_by_me;
	bool						m_dropped_by_me;
};

class server_connection_policy {
	
};

class server_role {
	typedef websocketpp::session::server connection_interface;
	typedef websocketpp::server::server_connection_policy connection_policy;
};

// ******* SSL SECURITY POLICY *******
//typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket;
	
/*class connection_ssl {
public:
	ssl_socket::lowest_layer_type& socket() {
		return m_socket.lowest_layer();
	}
	
	void handshake() {
		m_socket.async_handshake(
		    boost::asio::ssl::stream_base::server,
			boost::bind(
				&connection_ssl::handle_handshake,
				shared_from_this(),
				boost::asio::placeholders::error
			)
		);
	}
	
	void handle_handshake() {
		read_request();
	}
	
private:
	ssl_socket m_socket;
};
	
class endpoint_ssl {
public:
	typedef websocketpp::server::connection_ssl connection_policy;
	
private:
	boost::asio::ssl::context m_context;
};*/
// ******* END SSL SECURITY POLICY *******
	
// ******* PLAIN SECURITY POLICY *******
template <typename connection_type>
class connection_plain {
public:
	connection_plain(boost::asio::io_service& io_service) : m_socket(io_service) {}
	
	boost::asio::ip::tcp::socket& socket() {
		return m_socket;
	}
	
	void handshake() {
		connection_type::read_request();
	}
private:
	boost::asio::ip::tcp::socket m_socket;
};

template <typename xxx>
class endpoint_plain {
protected:	
	// base security class for connections that plain endpoints create
	template <typename connection_type>
	class connection_security_policy {
	public:
		connection_security_policy(boost::asio::io_service& io_service) : m_socket(io_service) {}
		
		typedef connection_security_policy<connection_type> foo;
		
		boost::asio::ip::tcp::socket& socket() {
			return m_socket;
		}
		
		void handshake(boost::shared_ptr<connection_type> foo) {
			//connection_type::read_request();
			//static_cast<connection_type*>(this)->read_request();
			foo->read_request();
			
			//boost::static_pointer_cast<connection_type>(foo::shared_from_this())
			
			//m_handler->on_close(boost::static_pointer_cast<websocketpp::session::server>(connection_type::shared_from_this()));
		}
	private:
		boost::asio::ip::tcp::socket m_socket;
	};
	
};
// ******* END PLAIN SECURITY POLICY *******

// TODO: potential policies:
// - http parser
template
	<
	template <class> class security_policy,
	template <class> class logger_type = log::logger
	>
class endpoint 
	: 
	public security_policy< endpoint<security_policy,logger_type> >,
	public boost::enable_shared_from_this< endpoint<security_policy,logger_type> > {
public:
	typedef endpoint<security_policy,logger_type> endpoint_type;
	typedef security_policy< endpoint<security_policy,logger_type> > security_type;
	
	typedef logger_type<log::alevel::value> alog_type;
	typedef logger_type<log::elevel::value> elog_type;
	
	using security_type::connection_security_policy;
	
	typedef connection<endpoint_type,security_type::template connection_security_policy> connection_type;
	
	typedef boost::shared_ptr<endpoint_type> ptr;
	//typedef websocketpp::session::server_ptr session_ptr;
	typedef boost::shared_ptr<connection_type> connection_ptr;
	
	endpoint<security_policy,logger_type>(uint16_t port, server_handler_ptr handler) 
	: m_endpoint(tcp::v6(),port),
	  m_acceptor(m_io_service,m_endpoint),
	  m_handler(handler),
	  m_max_message_size(DEFAULT_MAX_MESSAGE_SIZE),
	  m_desc("websocketpp::server") 
	{
		m_desc.add_options()
		  ("help", "produce help message")
		  ("host,h",po::value<std::vector<std::string> >()->multitoken()->composing(), "hostnames to listen on")
		  ("port,p",po::value<int>(), "port to listen on")
		;
	}
	
	void run() {
		start_accept();
		m_io_service.run();
	}
	
	
	
	// INTERFACE FOR LOCAL APPLICATIONS
	void set_max_message_size(uint64_t val) {
		if (val > frame::limits::PAYLOAD_SIZE_JUMBO) {
			// TODO: Figure out what the ideal error behavior for this method.
			// Options:
			//   Throw exception
			//   Log error and set value to maximum allowed
			//   Log error and leave value at whatever it was before
			elog().at(log::elevel::WARN) << "Invalid maximum message size: " 
			                          << val << log::endl;
			//throw server_error(err.str());
		}
		m_max_message_size = val;
	}
	
	void parse_command_line(int ac, char* av[]) {
		po::store(po::parse_command_line(ac,av, m_desc),m_vm);
		po::notify(m_vm);
		
		if (m_vm.count("help") ) {
			std::cout << m_desc << std::endl;
		}
		
		//m_vm["host"].as<std::string>();
		
		// TODO: template "as" weirdness 
		// const std::vector< std::string > &foo = m_vm["host"].as< std::vector<std::string> >();
		
		const std::vector< std::string > &foo = m_vm["host"].template as< std::vector<std::string> >();
		
		for (int i = 0; i < foo.size(); i++) {
			std::cout << foo[i] << std::endl;
		}
		
		//std::cout << m_vm["host"].as< std::vector<std::string> >() << std::endl;
	}
	
	// INTERFACE FOR SESSIONS
	
	static const bool is_server = true;
	
	/*rng_policy& get_rng() {
		return m_rng;
	}*/
	
	// Confirms that the port in the host string matches the port we are listening
	// on. End user application is responsible for checking the /host/ part.
	bool validate_host(std::string host) {
		// find colon.
		// if no colon assume default port
		
		// if port == port
		// return true
		// else
		// return false
		
		// TODO: just check the port. Otherwise user is responsible for checking this
		
		return true;
	}
	
	// Check if message size is within server's acceptable parameters
	bool validate_message_size(uint64_t val) {
		if (val > m_max_message_size) {
			return false;
		}
		return true;
	}
	
	logger_type<log::alevel::value>& alog() {
		return m_alog;
	}
	
	logger_type<log::elevel::value>& elog() {
		return m_elog;
	}
	
private:
	// creates a new session object and connects the next websocket
	// connection to it.
	void start_accept() {
		// TODO: sanity check whether the session buffer size bound could be reduced
		connection_ptr new_session(
			new connection_type(
				endpoint_type::shared_from_this(),
				m_io_service,
				m_handler
			)
		);
		
		m_acceptor.async_accept(
			new_session->socket(),
			boost::bind(
				&endpoint_type::handle_accept,
				endpoint_type::shared_from_this(),
				new_session,
				boost::asio::placeholders::error
			)
		);
	}
	
	// if no errors starts the session's read loop and returns to the
	// start_accept phase.
	void handle_accept(connection_ptr connection,const boost::system::error_code& error) 
	{
		if (!error) {
			connection->handshake(connection);
			
			// TODO: add session to local session vector
		} else {
			std::stringstream err;
			err << "Error accepting socket connection: " << error;
			
			elog().at(log::elevel::ERROR) << err.str() << log::endl;
			throw server_error(err.str());
		}
		
		this->start_accept();
	}
	
private:
	boost::asio::io_service	m_io_service;
	tcp::endpoint			m_endpoint;
	tcp::acceptor			m_acceptor;
	
	server_handler_ptr		m_handler;
	
	logger_type<log::alevel::value>	m_alog;
	logger_type<log::elevel::value> m_elog;
	
	std::vector<connection_ptr>	m_connections;
	
	uint64_t					m_max_message_size;	
	
	po::options_description		m_desc;
	po::variables_map			m_vm;
};
	
}
	
// convenience type interface

// websocketpp::server_ptr represents a basic non-secure websocket server
typedef server::endpoint<server::endpoint_plain> basic_server;
typedef basic_server::ptr basic_server_ptr;

// websocketpp::secure_server_ptr represents a basic secure websocket server
// TODO: 
//typedef server::server<> secure_server;
//typedef secure_server::ptr secure_server_ptr;
	
}

#endif // WEBSOCKET_SERVER_HPP
