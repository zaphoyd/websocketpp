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
#include "websocket_frame.hpp"

#include "websocket_server.hpp"
#include "utf8_validator/utf8_validator.hpp"

#include <iostream>
#include <algorithm>

#if defined(WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

using websocketpp::frame;


uint8_t frame::get_state() const {
	return m_state;
}

uint64_t frame::get_bytes_needed() const {
	return m_bytes_needed;
}

void frame::reset() {
	m_state = STATE_BASIC_HEADER;
	m_bytes_needed = BASIC_HEADER_LENGTH;
	m_degraded = false;
	m_payload.empty();
	memset(m_header,0,MAX_HEADER_LENGTH);
}

// Method invariant: One of the following must always be true even in the case 
// of exceptions.
// - m_bytes_needed > 0
// - m-state = STATE_READY
void frame::consume(std::istream &s) {
	try {
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
						
						if (m_bytes_needed == 0) {
							m_state = STATE_READY;
							process_payload();
							
						} else {
							m_state = STATE_PAYLOAD;
						}
					}
				}
				break;
			case STATE_EXTENDED_HEADER:
				s.read(&m_header[get_header_len()-m_bytes_needed],m_bytes_needed);
				
				m_bytes_needed -= s.gcount();
				
				if (m_bytes_needed == 0) {
					process_extended_header();
					if (m_bytes_needed == 0) {
						m_state = STATE_READY;
						process_payload();
					} else {
						m_state = STATE_PAYLOAD;
					}
				}
				break;
			case STATE_PAYLOAD:
				s.read(reinterpret_cast<char *>(&m_payload[m_payload.size()-m_bytes_needed]),
					   m_bytes_needed);
				
				m_bytes_needed -= s.gcount();
				
				if (m_bytes_needed == 0) {
					m_state = STATE_READY;
					process_payload();
				}
				break;
			case STATE_RECOVERY:
				// Recovery state discards all bytes that are not the first byte
				// of a close frame.
				do {
					s.read(reinterpret_cast<char *>(&m_header[0]),1);
					
					//std::cout << std::hex << int(static_cast<unsigned char>(m_header[0])) << " ";
					
					if (int(static_cast<unsigned char>(m_header[0])) == 0x88) {
						//(BPB0_FIN && CONNECTION_CLOSE)
						m_bytes_needed--;
						m_state = STATE_BASIC_HEADER;
						break;
					}
				} while (s.gcount() > 0);
				
				//std::cout << std::endl;
				
				break;
			default:
				break;
		}
		
		/*if (s.gcount() == 0) {
			throw frame_error("consume read zero bytes",FERR_FATAL_SESSION_ERROR);
		}*/
	} catch (const frame_error& e) {
		// After this point all non-close frames must be considered garbage, 
		// including the current one. Reset it and put the reading frame into
		// a recovery state.
		if (m_degraded == true) {
			throw frame_error("An error occurred while trying to gracefully recover from a less serious frame error.",FERR_FATAL_SESSION_ERROR);
		} else {
			reset();
			m_state = STATE_RECOVERY;
			m_degraded = true;
			
			throw e;
		}
	}
}

char* frame::get_header() {
	return m_header;
}

char* frame::get_extended_header() {
	return m_header+BASIC_HEADER_LENGTH;
}

unsigned int frame::get_header_len() const {
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

char* frame::get_masking_key() {
	if (m_state != STATE_READY) {
		throw frame_error("attempted to get masking_key before reading full header");
	}
	return m_masking_key;
}

// get and set header bits
bool frame::get_fin() const {
	return ((m_header[0] & BPB0_FIN) == BPB0_FIN);
}

void frame::set_fin(bool fin) {
	if (fin) {
		m_header[0] |= BPB0_FIN;
	} else {
		m_header[0] &= (0xFF ^ BPB0_FIN);
	}
}

// get and set reserved bits
bool frame::get_rsv1() const {
	return ((m_header[0] & BPB0_RSV1) == BPB0_RSV1);
}

void frame::set_rsv1(bool b) {
	if (b) {
		m_header[0] |= BPB0_RSV1;
	} else {
		m_header[0] &= (0xFF ^ BPB0_RSV1);
	}
}

bool frame::get_rsv2() const {
	return ((m_header[0] & BPB0_RSV2) == BPB0_RSV2);
}

void frame::set_rsv2(bool b) {
	if (b) {
		m_header[0] |= BPB0_RSV2;
	} else {
		m_header[0] &= (0xFF ^ BPB0_RSV2);
	}
}

bool frame::get_rsv3() const {
	return ((m_header[0] & BPB0_RSV3) == BPB0_RSV3);
}

void frame::set_rsv3(bool b) {
	if (b) {
		m_header[0] |= BPB0_RSV3;
	} else {
		m_header[0] &= (0xFF ^ BPB0_RSV3);
	}
}

frame::opcode frame::get_opcode() const {
	return frame::opcode(m_header[0] & BPB0_OPCODE);
}

void frame::set_opcode(frame::opcode op) {
	if (op > 0x0F) {
		throw frame_error("invalid opcode",FERR_PROTOCOL_VIOLATION);
	}
	
	if (get_basic_size() > BASIC_PAYLOAD_LIMIT && 
		is_control()) {
		throw frame_error("control frames can't have large payloads",FERR_PROTOCOL_VIOLATION);
	}
	
	m_header[0] &= (0xFF ^ BPB0_OPCODE); // clear op bits
	m_header[0] |= op; // set op bits
}

bool frame::get_masked() const {
	return ((m_header[1] & BPB1_MASK) == BPB1_MASK);
}

void frame::set_masked(bool masked) {
	if (masked) {
		m_header[1] |= BPB1_MASK;
		generate_masking_key();
	} else {
		m_header[1] &= (0xFF ^ BPB1_MASK);
		clear_masking_key();
	}
}

uint8_t frame::get_basic_size() const {
	return m_header[1] & BPB1_PAYLOAD;
}

size_t frame::get_payload_size() const {
	if (m_state != STATE_READY && m_state != STATE_PAYLOAD) {
		// problem
		throw frame_error("attempted to get payload size before reading full header");
	}
	
	return m_payload.size();
}

uint16_t frame::get_close_status() const {
	if (get_payload_size() == 0) {
		return close::status::NO_STATUS;
	} else if (get_payload_size() >= 2) {
		char val[2];
		
		val[0] = m_payload[0];
		val[1] = m_payload[1];
		
		uint16_t code = ntohs(*(
			reinterpret_cast<uint16_t*>(&val[0])
		));
		
		// these two codes should never be on the wire
		if (code == close::status::NO_STATUS || code == close::status::ABNORMAL_CLOSE) {
			throw frame_error("Invalid close status code on the wire");
		}
		
		return code;
	} else {
		return close::status::PROTOCOL_ERROR;
	}
}

std::string frame::get_close_msg() const {
	if (get_payload_size() > 2) {
		uint32_t state = utf8_validator::UTF8_ACCEPT;
		uint32_t codep = 0;
		validate_utf8(&state,&codep,2);
		if (state != utf8_validator::UTF8_ACCEPT) {
			throw frame_error("Invalid UTF-8 Data",
							  frame::FERR_PAYLOAD_VIOLATION);
		}
		return std::string(m_payload.begin()+2,m_payload.end());
	} else {
		return std::string();
	}
}

std::vector<unsigned char> &frame::get_payload() {
	return m_payload;
}

void frame::set_payload(const std::vector<unsigned char> source) {
	set_payload_helper(source.size());
	
	std::copy(source.begin(),source.end(),m_payload.begin());
}
void frame::set_payload(const std::string source) {
	set_payload_helper(source.size());
	
	std::copy(source.begin(),source.end(),m_payload.begin());
}

bool frame::is_control() const {
	return (get_opcode() > MAX_FRAME_OPCODE);
}

void frame::set_payload_helper(size_t s) {
	if (s > max_payload_size) {
		throw frame_error("requested payload is over implimentation defined limit",FERR_MSG_TOO_BIG);
	}
	
	// limits imposed by the websocket spec
	if (s > BASIC_PAYLOAD_LIMIT && 
		get_opcode() > MAX_FRAME_OPCODE) {
		throw frame_error("control frames can't have large payloads",FERR_PROTOCOL_VIOLATION);
	}
	
	if (s <= BASIC_PAYLOAD_LIMIT) {
		m_header[1] = s;
	} else if (s <= PAYLOAD_16BIT_LIMIT) {
		m_header[1] = BASIC_PAYLOAD_16BIT_CODE;
		
		// this reinterprets the second pair of bytes in m_header as a
		// 16 bit int and writes the payload size there as an integer
		// in network byte order
		*reinterpret_cast<uint16_t*>(&m_header[BASIC_HEADER_LENGTH]) = htons(s);
	} else if (s <= PAYLOAD_64BIT_LIMIT) {
		m_header[1] = BASIC_PAYLOAD_64BIT_CODE;
		*reinterpret_cast<uint64_t*>(&m_header[BASIC_HEADER_LENGTH]) = htonll(s);
	} else {
		throw frame_error("payload size limit is 63 bits",FERR_PROTOCOL_VIOLATION);
	}
	
	m_payload.resize(s);
}

void frame::set_status(uint16_t status,const std::string message) {
	// check for valid statuses
	if (close::status::invalid(status)) {
		std::stringstream err;
		err << "Status code " << status << " is invalid";
		throw frame_error(err.str());
	}
	
	if (close::status::reserved(status)) {
		std::stringstream err;
		err << "Status code " << status << " is reserved";
		throw frame_error(err.str());
	}
	
	m_payload.resize(2+message.size());
	
	char val[2];
	
	*reinterpret_cast<uint16_t*>(&val[0]) = htons(status);

	m_header[1] = message.size()+2;

	m_payload[0] = val[0];
	m_payload[1] = val[1];
	
	std::copy(message.begin(),message.end(),m_payload.begin()+2);
}

std::string frame::print_frame() const {
	std::stringstream f;
	
	unsigned int len = get_header_len();
	
	f << "frame: ";
	// print header
	for (unsigned int i = 0; i < len; i++) {
		f << std::hex << (unsigned short)m_header[i] << " ";
	}
	// print message
	if (m_payload.size() > 50) {
		f << "[payload of " << m_payload.size() << " bytes]";
	} else {
		std::vector<unsigned char>::const_iterator it;
		for (it = m_payload.begin(); it != m_payload.end(); it++) {
			f << *it;
		}
	}
	return f.str();
}

void frame::process_basic_header() {
	m_bytes_needed = get_header_len() - BASIC_HEADER_LENGTH;
}

void frame::process_extended_header() {
	uint8_t s = get_basic_size();
	uint64_t payload_size;
	int mask_index = BASIC_HEADER_LENGTH;
	
	if (s <= BASIC_PAYLOAD_LIMIT) {
		payload_size = s;
	} else if (s == BASIC_PAYLOAD_16BIT_CODE) {			
		// reinterpret the second two bytes as a 16 bit integer in network
		// byte order. Convert to host byte order and store locally.
		payload_size = ntohs(*(
			reinterpret_cast<uint16_t*>(&m_header[BASIC_HEADER_LENGTH])
		));
		
		if (payload_size < s) {
			std::stringstream err;
			err << "payload length not minimally encoded. Using 16 bit form for payload size: " << payload_size;
			m_bytes_needed = payload_size;
			throw frame_error(err.str(),
							  FERR_PROTOCOL_VIOLATION);
		}
		
		mask_index += 2;
	} else if (s == BASIC_PAYLOAD_64BIT_CODE) {
		// reinterpret the second eight bytes as a 64 bit integer in 
		// network byte order. Convert to host byte order and store.
		payload_size = ntohll(*(
			reinterpret_cast<uint64_t*>(&m_header[BASIC_HEADER_LENGTH])
		));
		
		if (payload_size <= PAYLOAD_16BIT_LIMIT) {
			m_bytes_needed = payload_size;
			throw frame_error("payload length not minimally encoded",
							  FERR_PROTOCOL_VIOLATION);
		}
		
		mask_index += 8;
	} else {
		// shouldn't be here
		throw frame_error("invalid get_basic_size in process_extended_header");
	}
	
	if (get_masked() == 0) {
		clear_masking_key();
	} else {
		// TODO: use this copy line (needs testing)
		// std::copy(m_header[mask_index],m_header[mask_index+4],m_masking_key);
		m_masking_key[0] = m_header[mask_index+0];
		m_masking_key[1] = m_header[mask_index+1];
		m_masking_key[2] = m_header[mask_index+2];
		m_masking_key[3] = m_header[mask_index+3];
	}
	
	if (payload_size > max_payload_size) {
		// TODO: frame/message size limits
		throw server_error("got frame with payload greater than maximum frame buffer size.");
	}
	m_payload.resize(payload_size);
	m_bytes_needed = payload_size;
}

void frame::process_payload() {
	if (get_masked()) {
		char *masking_key = &m_header[get_header_len()-4];
		
		for (uint64_t i = 0; i < m_payload.size(); i++) {
			m_payload[i] = (m_payload[i] ^ masking_key[i%4]);
		}
	}
}

void frame::process_payload2() {
	// unmask payload one byte at a time
	
	//uint64_t key = (*((uint32_t*)m_masking_key;)) << 32;
	//key += *((uint32_t*)m_masking_key);
	
	// might need to switch byte order
	uint32_t key = *((uint32_t*)m_masking_key);
	
	// 4
	
	uint64_t i = 0;
	uint64_t s = (m_payload.size() / 4);
	
	std::cout << "s: " << s << std::endl;
	
	// chunks of 4
	for (i = 0; i < s; i+=4) {
		((uint32_t*)(&m_payload[0]))[i] = (((uint32_t*)(&m_payload[0]))[i] ^ key);
	}
	
	// finish the last few
	for (i = s; i < m_payload.size(); i++) {
		m_payload[i] = (m_payload[i] ^ m_masking_key[i%4]);
	}
}

void frame::validate_utf8(uint32_t* state,uint32_t* codep, size_t offset) const {
	for (size_t i = offset; i < m_payload.size(); i++) {
		using utf8_validator::decode;
		
		if (decode(state,codep,m_payload[i]) == utf8_validator::UTF8_REJECT) {
			throw frame_error("Invalid UTF-8 Data",FERR_PAYLOAD_VIOLATION);
		}
	}
}

void frame::validate_basic_header() const {
	// check for control frame size
	if (get_basic_size() > BASIC_PAYLOAD_LIMIT && is_control()) {
		throw frame_error("Control Frame is too large",FERR_PROTOCOL_VIOLATION);
	}
	
	// check for reserved opcodes
	if (get_rsv1() || get_rsv2() || get_rsv3()) {
		throw frame_error("Reserved bit used",FERR_PROTOCOL_VIOLATION);
	}
	
	// check for reserved opcodes
	opcode op = get_opcode();
	if (op > 0x02 && op < 0x08) {
		throw frame_error("Reserved opcode used",FERR_PROTOCOL_VIOLATION);
	}
	if (op > 0x0A) {
		throw frame_error("Reserved opcode used",FERR_PROTOCOL_VIOLATION);
	}
	
	// check for fragmented control message
	if (is_control() && !get_fin()) {
		throw frame_error("Fragmented control message",FERR_PROTOCOL_VIOLATION);
	}
}

void frame::generate_masking_key() {
	//throw "masking key generation not implimented";
	
	int32_t key = m_gen();
		
	//m_masking_key[0] = reinterpret_cast<char*>(&key)[0];
	//m_masking_key[1] = reinterpret_cast<char*>(&key)[1];
	//m_masking_key[2] = reinterpret_cast<char*>(&key)[2];
	//m_masking_key[3] = reinterpret_cast<char*>(&key)[3];
	
	*(reinterpret_cast<int32_t *>(&m_header[get_header_len()-4])) = key;
	
	//std::cout << "maskkey: " << m_masking_key << std::endl;
	
	/* TODO: test and tune
	
	boost::random::random_device rng;
	boost::random::uniform_int_distribution<> mask(0,UINT32_MAX);
	
	*(reinterpret_cast<uint32_t *>(m_masking_key)) = mask(rng);
	
	*/
}

void frame::clear_masking_key() {
	// this is a no-op as clearing the mask bit also changes the get_header_len
	// method to not include these byte ranges. Whenever the masking bit is re-
	// set a new key is generated anyways.
}
