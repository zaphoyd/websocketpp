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

#ifndef WEBSOCKET_HYBI_PROCESSOR_HPP
#define WEBSOCKET_HYBI_PROCESSOR_HPP

#include "interfaces/protocol.hpp"

#include "websocket_frame.hpp"

#include "network_utilities.hpp"
#include "http/parser.hpp"

#include "base64/base64.h"
#include "sha1/sha1.h"

namespace websocketpp {
namespace protocol {

namespace hybi_state {
	enum value {
		INIT = 0,
		READ = 1,
		DONE = 2
	};
}

template <class rng_policy>
class hybi_processor : public processor {
public:
	hybi_processor(bool secure,rng_policy &rng) : m_secure(secure),m_fragmented_opcode(frame::opcode::CONTINUATION),m_utf8_payload(new utf8_string()),m_binary_payload(new binary_string()),m_read_frame(rng),m_write_frame(rng) {
		reset();
	}		
	
	void validate_handshake(const http::parser::request& request) const {
		std::stringstream err;
		std::string h;
		
		if (request.method() != "GET") {
			err << "Websocket handshake has invalid method: " 
			<< request.method();
			
			throw(http::exception(err.str(),http::status_code::BAD_REQUEST));
		}
		
		// TODO: allow versions greater than 1.1
		if (request.version() != "HTTP/1.1") {
			err << "Websocket handshake has invalid HTTP version: " 
			<< request.method();
			
			throw(http::exception(err.str(),http::status_code::BAD_REQUEST));
		}
		
		// verify the presence of required headers
		if (request.header("Host") == "") {
			throw(http::exception("Required Host header is missing",http::status_code::BAD_REQUEST));
		}
		
		h = request.header("Upgrade");
		if (h == "") {
			throw(http::exception("Required Upgrade header is missing",http::status_code::BAD_REQUEST));
		} else if (!boost::ifind_first(h,"websocket")) {
			err << "Upgrade header \"" << h << "\", does not contain required token \"websocket\"";
			throw(http::exception(err.str(),http::status_code::BAD_REQUEST));
		}
		
		h = request.header("Connection");
		if (h == "") {
			throw(http::exception("Required Connection header is missing",http::status_code::BAD_REQUEST));
		} else if (!boost::ifind_first(h,"upgrade")) {
			err << "Connection header, \"" << h 
			<< "\", does not contain required token \"upgrade\"";
			throw(http::exception(err.str(),http::status_code::BAD_REQUEST));
		}
		
		if (request.header("Sec-WebSocket-Key") == "") {
			throw(http::exception("Required Sec-WebSocket-Key header is missing",http::status_code::BAD_REQUEST));
		}
		
		h = request.header("Sec-WebSocket-Version");
		if (h == "") {
			throw(http::exception("Required Sec-WebSocket-Version header is missing",http::status_code::BAD_REQUEST));
		} else {
			int version = atoi(h.c_str());
			
			if (version != 7 && version != 8 && version != 13) {
				err << "This processor doesn't support WebSocket protocol version "
				<< version;
				throw(http::exception(err.str(),http::status_code::BAD_REQUEST));
			}
		}
	}
	
	std::string get_origin(const http::parser::request& request) const {
		std::string h = request.header("Sec-WebSocket-Version");
		int version = atoi(h.c_str());
				
		if (version == 13) {
			return request.header("Origin");
		} else if (version == 7 || version == 8) {
			return request.header("Sec-WebSocket-Origin");
		} else {
			throw(http::exception("Could not determine origin header. Check Sec-WebSocket-Version header",http::status_code::BAD_REQUEST));
		}
	}
	
	ws_uri get_uri(const http::parser::request& request) const {
		ws_uri uri;
		
		uri.secure = m_secure;
		
		
		std::string h = request.header("Host");
		
		size_t found = h.find(":");
		if (found == std::string::npos) {
			uri.host = h;
			uri.port = (m_secure ? DEFAULT_SECURE_PORT : DEFAULT_PORT);
		} else {
			uint16_t p = atoi(h.substr(found+1).c_str());
			
			if (p == 0) {
				throw(http::exception("Could not determine request uri. Check host header.",http::status_code::BAD_REQUEST));
			} else {
				uri.host = h.substr(0,found);
				uri.port = p;
			}
		}
		
		// TODO: check if get_uri is a full uri
		uri.resource = request.uri();
		
		std::cout << "parsed uri: " << uri.str() << std::endl;
		
		return uri;
	}
	
	void handshake_response(const http::parser::request& request,http::parser::response& response) {
		// TODO:
		
		std::string server_key = request.header("Sec-WebSocket-Key");
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
				reinterpret_cast<const unsigned char*>
					(message_digest),20
			);
			
			// set handshake accept headers
			response.replace_header("Sec-WebSocket-Accept",server_key);
			response.add_header("Upgrade","websocket");
			response.add_header("Connection","Upgrade");
		} else {
			//m_endpoint->elog().at(log::elevel::ERROR) 
			//<< "Error computing handshake sha1 hash" << log::endl;
			// TODO: make sure this error path works
			response.set_status(http::status_code::INTERNAL_SERVER_ERROR);
		}
	}
	
	void consume(std::istream& s) {
		while (s.good() && m_state != hybi_state::DONE) {
			try {
				m_read_frame.consume(s);
				
				if (m_read_frame.ready()) {
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
							if (!utf8_validator::validate(m_read_frame.get_close_msg())) {
								throw session::exception("Invalid UTF8",session::error::PAYLOAD_VIOLATION);
							}
							
							m_opcode = frame::opcode::CLOSE;
							m_close_code = m_read_frame.get_close_status();
							m_close_reason = m_read_frame.get_close_msg();
							
							break;
						case frame::opcode::PING:
						case frame::opcode::PONG:
							m_opcode = m_read_frame.get_opcode();
							extract_binary(m_control_payload);
							break;
						default:
							throw session::exception("Invalid Opcode",session::error::PROTOCOL_VIOLATION);
							break;
					}
					if (m_read_frame.get_fin()) {
						m_state = hybi_state::DONE;
						if (m_opcode == frame::opcode::TEXT) {
							if (!m_validator.complete()) {
								m_validator.reset();
								throw session::exception("Invalid UTF8",session::error::PAYLOAD_VIOLATION);
							}
							m_validator.reset();
						}
					}
					m_read_frame.reset();
				}
			} catch (const frame::exception& e) {
				if (m_read_frame.ready()) {
					m_read_frame.reset();
				}
				
				throw session::exception("Frame Error",session::error::PROTOCOL_VIOLATION);
			}
		}
	}
	
	// frame type handlers:
	void process_continuation() {
		if (m_fragmented_opcode == frame::opcode::BINARY) {
			extract_binary(m_binary_payload);
		} else if (m_fragmented_opcode == frame::opcode::TEXT) {
			extract_utf8(m_utf8_payload);
		} else if (m_fragmented_opcode == frame::opcode::CONTINUATION) {
			// got continuation frame without a message to continue.
			throw session::exception("No message to continue.",session::error::PROTOCOL_VIOLATION);
		} else {
			
			// can't be here
		}
		if (m_read_frame.get_fin()) {
			m_opcode = m_fragmented_opcode;
		}
	}
	
	void process_text() {
		if (m_fragmented_opcode != frame::opcode::CONTINUATION) {
			throw session::exception("New message started without closing previous.",session::error::PROTOCOL_VIOLATION);
		}
		extract_utf8(m_utf8_payload);
		m_opcode = frame::opcode::TEXT;
		m_fragmented_opcode = frame::opcode::TEXT;
	}
	
	void process_binary() {
		if (m_fragmented_opcode != frame::opcode::CONTINUATION) {
			throw session::exception("New message started without closing previous.",session::error::PROTOCOL_VIOLATION);
		}
		m_opcode = frame::opcode::BINARY;
		m_fragmented_opcode = frame::opcode::BINARY;
		extract_binary(m_binary_payload);
	}
	
	void extract_binary(binary_string_ptr dest) {
		binary_string &msg = m_read_frame.get_payload();
		dest->resize(dest->size() + msg.size());
		std::copy(msg.begin(),msg.end(),dest->end() - msg.size());
	}
	
	void extract_utf8(utf8_string_ptr dest) {
		binary_string &msg = m_read_frame.get_payload();
		
		if (!m_validator.decode(msg.begin(),msg.end())) {
			throw session::exception("Invalid UTF8",session::error::PAYLOAD_VIOLATION);
		}
		
		dest->reserve(dest->size() + msg.size());
		dest->append(msg.begin(),msg.end());
	}
	
	bool ready() const {
		return m_state == hybi_state::DONE;
	}
	
	void reset() {
		m_state = m_state = hybi_state::INIT;
		m_control_payload = binary_string_ptr(new binary_string());
		
		if (m_fragmented_opcode == m_opcode) {
			m_utf8_payload = utf8_string_ptr(new utf8_string());
			m_binary_payload = binary_string_ptr(new binary_string());
			m_fragmented_opcode = frame::opcode::CONTINUATION;
		}
	}
	
	uint64_t get_bytes_needed() const {
		return m_read_frame.get_bytes_needed();
	}
	
	frame::opcode::value get_opcode() const {
		if (!ready()) {
			throw "not ready";
		}
		return m_opcode;
	}
	
	utf8_string_ptr get_utf8_payload() const {
		if (get_opcode() != frame::opcode::TEXT) {
			throw "opcode doesn't have a utf8 payload";
		}
		
		if (!ready()) {
			throw "not ready";
		}
		
		return m_utf8_payload;
	}
	
	binary_string_ptr get_binary_payload() const {
		if (!ready()) {
			throw "not ready";
		}
		
		if (get_opcode() == frame::opcode::BINARY) {
			return m_binary_payload;
		} else if (get_opcode() != frame::opcode::PING ||
				   get_opcode() != frame::opcode::PONG) {
			return m_control_payload;
		} else {
			throw "opcode doesn't have a binary payload";
		}
	}
	
	// legacy hybi doesn't have close codes
	close::status::value get_close_code() const {
		if (!ready()) {
			throw "not ready";
		}
		
		return m_close_code;
	}
	
	utf8_string get_close_reason() const {
		if (!ready()) {
			throw "not ready";
		}

		return m_close_reason;
	}
	
	binary_string_ptr prepare_frame(frame::opcode::value opcode,
									bool mask,
									const utf8_string& payload) {
		if (opcode != frame::opcode::TEXT) {
			// TODO: hybi_legacy doesn't allow non-text frames.
			throw;
		}
				
		// TODO: utf8 validation on payload.
		
		binary_string_ptr response(new binary_string(0));
		
		m_write_frame.reset();
		m_write_frame.set_opcode(opcode);
		m_write_frame.set_masked(mask);
		m_write_frame.set_fin(true);
		m_write_frame.set_payload(payload);
		
		// TODO
		response->resize(m_write_frame.get_header_len()+m_write_frame.get_payload().size());
		
		// copy header
		std::copy(m_write_frame.get_header(),m_write_frame.get_header()+m_write_frame.get_header_len(),response->begin());
		
		// copy payload
		std::copy(m_write_frame.get_payload().begin(),m_write_frame.get_payload().end(),response->begin()+m_write_frame.get_header_len());

		
		return response;
	}
	
	binary_string_ptr prepare_frame(frame::opcode::value opcode,
									bool mask,
									const binary_string& payload) {
		/*if (opcode != frame::opcode::TEXT) {
			// TODO: hybi_legacy doesn't allow non-text frames.
			throw;
		}*/
				
		// TODO: utf8 validation on payload.
		
		binary_string_ptr response(new binary_string(0));
		
		m_write_frame.reset();
		m_write_frame.set_opcode(opcode);
		m_write_frame.set_masked(mask);
		m_write_frame.set_fin(true);
		m_write_frame.set_payload(payload);
		
		// TODO
		response->resize(m_write_frame.get_header_len()+m_write_frame.get_payload().size());
		
		// copy header
		std::copy(m_write_frame.get_header(),m_write_frame.get_header()+m_write_frame.get_header_len(),response->begin());
		
		// copy payload
		std::copy(m_write_frame.get_payload().begin(),m_write_frame.get_payload().end(),response->begin()+m_write_frame.get_header_len());
		
		return response;
	}
	
	binary_string_ptr prepare_close_frame(close::status::value code,
										  bool mask,
										  const std::string& reason) {
		binary_string_ptr response(new binary_string(0));
		
		m_write_frame.reset();
		m_write_frame.set_opcode(frame::opcode::CLOSE);
		m_write_frame.set_masked(mask);
		m_write_frame.set_fin(true);
		m_write_frame.set_status(code,reason);
		
		// TODO
		response->resize(m_write_frame.get_header_len()+m_write_frame.get_payload().size());
		
		// copy header
		std::copy(m_write_frame.get_header(),m_write_frame.get_header()+m_write_frame.get_header_len(),response->begin());
		
		// copy payload
		std::copy(m_write_frame.get_payload().begin(),m_write_frame.get_payload().end(),response->begin()+m_write_frame.get_header_len());
		
		return response;
	}
	
private:
	bool					m_secure;
	int						m_state;
	frame::opcode::value	m_opcode;
	frame::opcode::value	m_fragmented_opcode;
	
	utf8_string_ptr			m_utf8_payload;
	binary_string_ptr		m_binary_payload;
	binary_string_ptr		m_control_payload;
	
	close::status::value	m_close_code;
	std::string				m_close_reason;
	
	utf8_validator::validator	m_validator;
	
	frame::parser<rng_policy>	m_read_frame;
	frame::parser<rng_policy>	m_write_frame;
};

//typedef boost::shared_ptr<hybi_processor> hybi_processor_ptr;
	
}
}
#endif // WEBSOCKET_HYBI_PROCESSOR_HPP
