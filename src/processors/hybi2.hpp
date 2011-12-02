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

#ifndef WEBSOCKET_PROCESSOR_HYBI_HPP
#define WEBSOCKET_PROCESSOR_HYBI_HPP

#include "processor.hpp"

#include "../base64/base64.h"
#include "../sha1/sha1.h"

#include <boost/algorithm/string.hpp>


namespace websocketpp {
namespace processor {

namespace hybi_state {
	enum value {
		READ_HEADER = 0,
		READ_PAYLOAD = 1,
		READY = 2
	};
}

// connection must provide:
// int32_t get_rng();
// message::data_ptr get_data_message();
// message::control_ptr get_control_message();
// bool get_secure();
	
template <class connection_type>
class hybi : public processor_base {
public:
	hybi(connection_type &connection) 
	 : m_connection(connection),
	   m_fragmented_opcode(frame::opcode::CONTINUATION),
	   m_utf8_payload(new utf8_string()),
	   m_binary_payload(new binary_string()),
	   m_read_frame(rng),m_write_frame(rng) 
	{
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
	
	uri_ptr get_uri(const http::parser::request& request) const {
		//uri connection_uri;
		
		
		//connection_uri.secure = m_connection.get_secure();
		
		std::string h = request.header("Host");
		
		//std::string host;
		//std::string port;
		
		size_t found = h.find(":");
		if (found == std::string::npos) {
			return uri_ptr(new uri(m_connection.get_secure(),h,request.uri()));
		} else {
			return uri_ptr(new uri(m_connection.get_secure(),h.substr(0,found),h.substr(found+1),request.uri()));
			
			/*uint16_t p = atoi(h.substr(found+1).c_str());
			
			if (p == 0) {
				throw(http::exception("Could not determine request uri. Check host header.",http::status_code::BAD_REQUEST));
			} else {
				connection_uri.host = h.substr(0,found);
				connection_uri.port = p;
			}*/
		}
		
		// TODO: check if get_uri is a full uri
		//connection_uri.resource = request.uri();
		
		//return uri(m_connection.get_secure(),host,port,request.uri());
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
	
	void consume(std::istream& s)
	{
		while (s.good() && m_state != hybi_state::READY) {
			try {
				switch (m_state) {
					case hybi_state::READ_HEADER:
						process_header();
						break;
					case hybi_state::READ_PAYLOAD:
						process_payload();
						break;
					case hybi_state::READY:
						// shouldn't be here..
						break;
					default:
						break;
				}
			} catch (const processor::exception& e) {
				if (m_header.ready()) {
					m_header.reset();
				}
				
				throw e;
			}
		}
	}
	
	void process_header() {
		m_header.consume(s);
		
		if (m_header.ready()) {
			// Get a free message from the read queue for the type of the 
			// current message
			if (m_header.is_control()) {
				// get a control message
				m_control_message = m_connection.get_control_message();
				
				if (!m_control_message) {
					throw "no control messages avaliable for reading.";
				}
				
			} else {
				m_data_message = m_connection.get_data_message();
				
				if (!m_data_message) {
					throw "no control messages avaliable for reading.";
				}
				
				m_data_message.reset(m_header.get_opcode(),
									 m_header.get_masking_key());
			}
			
			m_payload_left = m_header.get_payload_size();
			
			if (m_payload_left == 0) {
				m_state = hybi_state::READY;
			}
		}
	}
	
	void process_payload() {
		uint64_t written;
		if (m_header.is_control()) {
			written = m_control_message.consume(s,m_payload_left);
			m_payload_left -= written;
			
			if (m_payload_left == 0) {
				m_state = hybi_state::READY;
			}
			
		} else {
			written = m_data_message.consume(s,m_payload_left);
			m_payload_left -= written;
			
			if (m_payload_left == 0) {
				m_data_message.complete();
				m_state = hybi_state::READY;
			}
		}
		
	}
	
	bool ready() const {
		return m_state == hybi_state::READY;
	}
	
	bool is_control() const {
		return m_header.is_control();
	}
	
	message::data_ptr get_data_message() const {
		return m_data_message;
	}
	
	message::control_ptr get_control_message() const {
		return m_control_message;
	}
	
	void reset() {
		m_state = m_state = hybi_state::READ_HEADER;
		m_header.reset();
	}
	
	uint64_t get_bytes_needed() const {
		switch (m_state) {
			case hybi_state::READ_HEADER:
				return m_header.get_bytes_needed();
			case hybi_state::READ_PAYLOAD:
				return m_payload_left;
			case hybi_state::READY:
				return 0;
			default:
				throw "shouldn't be here";
		}
	}
	
	
	
	
	
	
	
	
	
	void process_frame() {
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
					throw processor::exception("Invalid UTF8",processor::error::PAYLOAD_VIOLATION);
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
				throw processor::exception("Invalid Opcode",processor::error::PROTOCOL_VIOLATION);
				break;
		}
		if (m_read_frame.get_fin()) {
			m_state = hybi_state::DONE;
			if (m_opcode == frame::opcode::TEXT) {
				if (!m_validator.complete()) {
					m_validator.reset();
					throw processor::exception("Invalid UTF8",processor::error::PAYLOAD_VIOLATION);
				}
				m_validator.reset();
			}
		}
		m_read_frame.reset();
	}
	
	// frame type handlers:
	void process_continuation() {
		if (m_fragmented_opcode == frame::opcode::BINARY) {
			extract_binary(m_binary_payload);
		} else if (m_fragmented_opcode == frame::opcode::TEXT) {
			extract_utf8(m_utf8_payload);
		} else if (m_fragmented_opcode == frame::opcode::CONTINUATION) {
			// got continuation frame without a message to continue.
			throw processor::exception("No message to continue.",processor::error::PROTOCOL_VIOLATION);
		} else {
			
			// can't be here
		}
		if (m_read_frame.get_fin()) {
			m_opcode = m_fragmented_opcode;
		}
	}
	
	void process_text() {
		if (m_fragmented_opcode != frame::opcode::CONTINUATION) {
			throw processor::exception("New message started without closing previous.",processor::error::PROTOCOL_VIOLATION);
		}
		extract_utf8(m_utf8_payload);
		m_opcode = frame::opcode::TEXT;
		m_fragmented_opcode = frame::opcode::TEXT;
	}
	
	void process_binary() {
		if (m_fragmented_opcode != frame::opcode::CONTINUATION) {
			throw processor::exception("New message started without closing previous.",processor::error::PROTOCOL_VIOLATION);
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
			throw processor::exception("Invalid UTF8",processor::error::PAYLOAD_VIOLATION);
		}
		
		dest->reserve(dest->size() + msg.size());
		dest->append(msg.begin(),msg.end());
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
	connection_type&		m_connection;
	int						m_state;
	frame::opcode::value	m_opcode;
	frame::opcode::value	m_fragmented_opcode;
	
	message::data_ptr		m_data_message;
	message::control_ptr	m_control_message;
	header					m_header;
	uint64_t				m_payload_left;
	
	utf8_string_ptr			m_utf8_payload;
	binary_string_ptr		m_binary_payload;
	binary_string_ptr		m_control_payload;
	
	close::status::value	m_close_code;
	std::string				m_close_reason;
	
	utf8_validator::validator	m_validator;
	
	frame::parser<rng_policy>	m_read_frame;
	frame::parser<rng_policy>	m_write_frame;
};

class header {
public:
	header() {
		reset();
	}
	
	uint64_t get_bytes_needed() const {
		return m_bytes_needed;
	}
	
	void reset() {
		m_state = STATE_BASIC_HEADER;
		m_bytes_needed = BASIC_HEADER_LENGTH;
	}
	
	void consume(std::istream& input) {
		switch (m_state) {
			case STATE_BASIC_HEADER:
				s.read(&m_header[BASIC_HEADER_LENGTH-m_bytes_needed],m_bytes_needed);
				
				m_bytes_needed -= s.gcount();
				
				if (m_bytes_needed == 0) {
					process_basic_header();
					
					validate_basic_header();
					
					if (m_bytes_needed > 0) {
						m_state = STATE_EXTENDED_HEADER;
					} else {
						process_extended_header();
						m_state = STATE_READY;
					}
				}
				break;
			case STATE_EXTENDED_HEADER:
				s.read(&m_header[get_header_len()-m_bytes_needed],m_bytes_needed);
				
				m_bytes_needed -= s.gcount();
				
				if (m_bytes_needed == 0) {
					process_extended_header();
					m_state = STATE_READY;
				}
				break;
			default:
				break;
		}
	}
	
	unsigned int get_header_len() const {
		unsigned int temp = 2;
		
		if (get_masked()) {
			temp += 4;
		}
		
		if (get_basic_size() == 126) {
			temp += 2;
		} else if (get_basic_size() == 127) {
			temp += 8;
		}
		
		return temp;
	}
	
	char* get_masking_key() {
		if (m_state != STATE_READY) {
			throw processor::exception("attempted to get masking_key before reading full header");
		}
		return m_header[get_header_len()-4];
	}
	
	// get and set header bits
	bool get_fin() const {
		return ((m_header[0] & BPB0_FIN) == BPB0_FIN);
	}
	void set_fin(bool fin) {
		if (fin) {
			m_header[0] |= BPB0_FIN;
		} else {
			m_header[0] &= (0xFF ^ BPB0_FIN);
		}
	}
	
	bool get_rsv1() const {
		return ((m_header[0] & BPB0_RSV1) == BPB0_RSV1);
	}
	void set_rsv1(bool b) {
		if (b) {
			m_header[0] |= BPB0_RSV1;
		} else {
			m_header[0] &= (0xFF ^ BPB0_RSV1);
		}
	}
	
	bool get_rsv2() const {
		return ((m_header[0] & BPB0_RSV2) == BPB0_RSV2);
	}
	void set_rsv2(bool b) {
		if (b) {
			m_header[0] |= BPB0_RSV2;
		} else {
			m_header[0] &= (0xFF ^ BPB0_RSV2);
		}
	}
	
	bool get_rsv3() const {
		return ((m_header[0] & BPB0_RSV3) == BPB0_RSV3);
	}
	void set_rsv3(bool b) {
		if (b) {
			m_header[0] |= BPB0_RSV3;
		} else {
			m_header[0] &= (0xFF ^ BPB0_RSV3);
		}
	}
	
	opcode::value get_opcode() const {
		return frame::opcode::value(m_header[0] & BPB0_OPCODE);
	}
	void set_opcode(opcode::value op) {
		if (opcode::reserved(op)) {
			throw processor::exception("reserved opcode",processor::error::PROTOCOL_VIOLATION);
		}
		
		if (opcode::invalid(op)) {
			throw processor::exception("invalid opcode",processor::error::PROTOCOL_VIOLATION);
		}
		
		if (is_control() && get_basic_size() > limits::PAYLOAD_SIZE_BASIC) {
			throw processor::exception("control frames can't have large payloads",processor::error::PROTOCOL_VIOLATION);
		}
		
		m_header[0] &= (0xFF ^ BPB0_OPCODE); // clear op bits
		m_header[0] |= op; // set op bits
	}
	
	bool get_masked() const {
		return ((m_header[1] & BPB1_MASK) == BPB1_MASK);
	}
	void set_masked(bool masked) {
		if (masked) {
			m_header[1] |= BPB1_MASK;
			generate_masking_key();
		} else {
			m_header[1] &= (0xFF ^ BPB1_MASK);
			clear_masking_key();
		}
	}
	
	uint8_t get_basic_size() const {
		return m_header[1] & BPB1_PAYLOAD;
	}
	size_t get_payload_size() const {
		if (m_state != STATE_READY && m_state != STATE_PAYLOAD) {
			// TODO: how to handle errors like this?
			throw "attempted to get payload size before reading full header";
		}
		
		return m_payload.size();
	}
	
	bool is_control() const {
		return (opcode::is_control(get_opcode()));
	}
	
	void process_basic_header() {
		m_bytes_needed = get_header_len() - BASIC_HEADER_LENGTH;
	}
	void process_extended_header() {
		uint8_t s = get_basic_size();
		
		if (s <= limits::PAYLOAD_SIZE_BASIC) {
			m_payload_size = s;
		} else if (s == BASIC_PAYLOAD_16BIT_CODE) {			
			// reinterpret the second two bytes as a 16 bit integer in network
			// byte order. Convert to host byte order and store locally.
			m_payload_size = ntohs(*(
				reinterpret_cast<uint16_t*>(&m_header[BASIC_HEADER_LENGTH])
			));
			
			if (m_payload_size < s) {
				std::stringstream err;
				err << "payload length not minimally encoded. Using 16 bit form for payload size: " << m_payload_size;
				throw processor::exception(err.str(),processor::error::PROTOCOL_VIOLATION);
			}
			
		} else if (s == BASIC_PAYLOAD_64BIT_CODE) {
			// reinterpret the second eight bytes as a 64 bit integer in 
			// network byte order. Convert to host byte order and store.
			m_payload_size = ntohll(*(
				reinterpret_cast<uint64_t*>(&m_header[BASIC_HEADER_LENGTH])
			));
			
			if (m_payload_size <= limits::PAYLOAD_SIZE_EXTENDED) {
				throw processor::exception("payload length not minimally encoded",
										   processor::error::PROTOCOL_VIOLATION);
			}
			
		} else {
			// TODO: shouldn't be here how to handle?
			throw processor::exception("invalid get_basic_size in process_extended_header");
		}
	}
	
	void validate_basic_header() const {
		// check for control frame size
		if (is_control() && get_basic_size() > limits::PAYLOAD_SIZE_BASIC) {
			throw processor::exception("Control Frame is too large",processor::error::PROTOCOL_VIOLATION);
		}
		
		// check for reserved bits
		if (get_rsv1() || get_rsv2() || get_rsv3()) {
			throw processor::exception("Reserved bit used",processor::error::PROTOCOL_VIOLATION);
		}
		
		// check for reserved opcodes
		if (opcode::reserved(get_opcode())) {
			throw processor::exception("Reserved opcode used",processor::error::PROTOCOL_VIOLATION);
		}
		
		// check for fragmented control message
		if (is_control() && !get_fin()) {
			throw processor::exception("Fragmented control message",processor::error::PROTOCOL_VIOLATION);
		}
	}
	
	void set_masking_key(int32_t key) {
		*(reinterpret_cast<int32_t *>(&m_header[get_header_len()-4])) = key;
	}
	void clear_masking_key() {
		// this is a no-op as clearing the mask bit also changes the get_header_len
		// method to not include these byte ranges. Whenever the masking bit is re-
		// set a new key is generated anyways.
	}
	
private:
	// basic payload byte flags
	static const uint8_t BPB0_OPCODE = 0x0F;
	static const uint8_t BPB0_RSV3 = 0x10;
	static const uint8_t BPB0_RSV2 = 0x20;
	static const uint8_t BPB0_RSV1 = 0x40;
	static const uint8_t BPB0_FIN = 0x80;
	static const uint8_t BPB1_PAYLOAD = 0x7F;
	static const uint8_t BPB1_MASK = 0x80;
	
	static const uint8_t BASIC_PAYLOAD_16BIT_CODE = 0x7E; // 126
	static const uint8_t BASIC_PAYLOAD_64BIT_CODE = 0x7F; // 127
	
	static const unsigned int BASIC_HEADER_LENGTH = 2;		
	static const unsigned int MAX_HEADER_LENGTH = 14;
	
	static const uint8_t STATE_BASIC_HEADER = 1;
	static const uint8_t STATE_EXTENDED_HEADER = 2;
	static const uint8_t STATE_READY = 3;
	
	uint8_t		m_state;
	uint64_t	m_bytes_needed;
	uint64_t	m_payload_size;
	char m_header[MAX_HEADER_LENGTH];
};
	
	
} // namespace processor
} // namespace websocketpp

#endif // WEBSOCKET_PROCESSOR_HYBI_HPP
