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

#ifndef WEBSOCKET_HYBI_LEGACY_PROCESSOR_HPP
#define WEBSOCKET_HYBI_LEGACY_PROCESSOR_HPP

#include "interfaces/protocol.hpp"

#include "network_utilities.hpp"

namespace websocketpp {
namespace protocol {

namespace hybi_legacy_state {
	enum value {
		INIT = 0,
		READ = 1,
		DONE = 2
	};
}
	
class hybi_legacy_processor : public processor {
public:
	hybi_legacy_processor() : m_state(hybi_legacy_state::INIT) {
		reset();
	}
	
	void validate_handshake(const http::parser::request& headers) const {
		
	}
	
	void handshake_response(const http::parser::request& request,http::parser::response& response) {
		char key_final[16];
		
		// key1
		*reinterpret_cast<uint32_t*>(&key_final[0]) = 
		    decode_client_key(request.header("Sec-WebSocket-Key1"));
		
		// key2
		*reinterpret_cast<uint32_t*>(&key_final[4]) = 
		    decode_client_key(request.header("Sec-WebSocket-Key2"));
		
		// key3
		memcpy(&key_final[8],request.header("Sec-WebSocket-Key3").c_str(),8);
		
		// md5
		md5_hash_string(key_final,m_digest);
		m_digest[16] = 0;
		
		response.add_header("Upgrade","websocket");
		response.add_header("Connection","Upgrade");
		
		// TODO: require headers that need application specific information?
		
		// Echo back client's origin unless our local application set a
		// more restrictive one.
		if (response.header("Sec-WebSocket-Origin") == "") {
			response.add_header("Sec-WebSocket-Origin",request.header("Origin"));
		}
		
		// Echo back the client's request host unless our local application
		// set a different one.
		if (response.header("Sec-WebSocket-Location") == "") {
			// TODO: extract from host header rather than hard code
			/*ws_uri uri;
			uri.secure = false;
			
			std::string h = request.get_header("Host");
			
			size_t found = h.find(":");
			if (found == string::npos) {
				uri.host = h;
				uri.port = 80;
			} else {
				uri.host = h.substr();
			}
			
			
			uri.port = 9002;*/
			
			std::string h = "ws://"+request.header("Host")+"/";
			
			response.add_header("Sec-WebSocket-Location",h);
		}
	}
	
	void consume(std::istream& s) {
		unsigned char c;
		while (s.good() && m_state != hybi_legacy_state::DONE) {
			c = s.get();
			if (s.good()) {
				process(c);
			}
		}
	}
	
	void process(unsigned char c) {
		if (m_state == hybi_legacy_state::INIT) {
			// we are looking for a 0x00
			if (c == 0x00) {
				// start a message
				
				m_state = hybi_legacy_state::READ;
			} else {
				// TODO: ignore or error
				throw session::exception("invalid character read",session::error::PROTOCOL_VIOLATION);
			}
		} else if (m_state == hybi_legacy_state::READ) {
			// TODO: utf8 validation
			
			if (c == 0xFF) {
				// end
				m_state = hybi_legacy_state::DONE;
			} else {
				m_utf8_payload->push_back(c);
			}
		}
	}
	
	bool ready() const {
		return m_state == hybi_legacy_state::DONE;
	}
	
	void reset() {
		m_state = hybi_legacy_state::INIT;
		m_utf8_payload = utf8_string_ptr(new utf8_string());
	}
	
	uint64_t get_bytes_needed() const {
		return 1;
	}
	
	frame::opcode::value get_opcode() const {
		return frame::opcode::TEXT;
	}
	
	std::string get_key3() const {
		return std::string(m_digest);
	}
	
	utf8_string_ptr get_utf8_payload() const {
		if (get_opcode() != frame::opcode::TEXT) {
			throw "opcode doesn't match";
		}
		
		if (!ready()) {
			throw "not ready";
		}
		
		return m_utf8_payload;
	}
	
	binary_string_ptr get_binary_payload() const {
		// TODO: opcode doesn't match
		throw "opcode doesn't match";
		return binary_string_ptr();
	}
	
	// legacy hybi doesn't have close codes
	close::status::value get_close_code() const {
		return close::status::NO_STATUS;
	}
	
	utf8_string get_close_reason() const {
		return "";
	}
	
	binary_string_ptr prepare_frame(frame::opcode::value opcode,
									bool mask,
									const binary_string& payload)  {
		if (opcode != frame::opcode::TEXT) {
			// TODO: hybi_legacy doesn't allow non-text frames.
			throw;
		}
		
		// TODO: mask = ignore?
		
		// TODO: utf8 validation on payload.
		
		binary_string_ptr response(new binary_string(payload.size()+2));
		
		(*response)[0] = 0x00;
		std::copy(payload.begin(),payload.end(),response->begin()+1);
		(*response)[response->size()-1] = 0xFF;
		
		return response;
	}
	
	binary_string_ptr prepare_frame(frame::opcode::value opcode,
									bool mask,
									const utf8_string& payload)  {
		if (opcode != frame::opcode::TEXT) {
			// TODO: hybi_legacy doesn't allow non-text frames.
			throw;
		}
		
		// TODO: mask = ignore?
		
		// TODO: utf8 validation on payload.
		
		binary_string_ptr response(new binary_string(payload.size()+2));
		
		(*response)[0] = 0x00;
		std::copy(payload.begin(),payload.end(),response->begin()+1);
		(*response)[response->size()-1] = 0xFF;
		
		return response;
	}
	
	binary_string_ptr prepare_close_frame(close::status::value code,
										  bool mask,
										  const std::string& reason) {
		binary_string_ptr response(new binary_string(2));
		
		(*response)[0] = 0xFF;
		(*response)[1] = 0x00;
		
		return response;
	}
	
private:
	uint32_t decode_client_key(const std::string& key) {
		int spaces = 0;
		std::string digits = "";
		uint32_t num;
		
		// key2
		for (size_t i = 0; i < key.size(); i++) {
			if (key[i] == ' ') {
				spaces++;
			} else if (key[i] >= '0' && key[i] <= '9') {
				digits += key[i];
			}
		}
		
		num = atoi(digits.c_str());
		if (spaces > 0 && num > 0) {
			return htonl(num/spaces);
		} else {
			return 0;
		}
	}
	
	hybi_legacy_state::value	m_state;
	frame::opcode::value		m_opcode;
	
	utf8_string_ptr				m_utf8_payload;
	
	char						m_digest[17];
};

typedef boost::shared_ptr<hybi_legacy_processor> hybi_legacy_processor_ptr;
	
}
}
#endif // WEBSOCKET_HYBI_LEGACY_PROCESSOR_HPP
