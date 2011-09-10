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

#include "websocket_frame.hpp"
#include "websocket_server.hpp"

#include <iostream>
#include <algorithm>

#include <arpa/inet.h>

using websocketpp::frame;

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
	if (m_extended_header_bytes_needed > 0) {
		throw "attempted to get masking_key before reading full header";
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
		throw "invalid opcode";
	}
	
	if (get_basic_size() > BASIC_PAYLOAD_LIMIT && 
		is_control()) {
		throw "control frames can't have large payloads";
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
	if (m_extended_header_bytes_needed > 0) {
		// problem
		throw "attempted to get payload size before reading full header";
	}
	
	return m_payload.size();
}

uint16_t frame::get_close_status() const {
	if (get_payload_size() >= 2) {
		char val[2];
		
		val[0] = m_payload[0];
		val[1] = m_payload[1];
		
		uint16_t code = ntohs(*(
			reinterpret_cast<uint16_t*>(&val[0])
		));
		
		return code;
	} else {
		return 1005; // defined in spec as "no status recieved"
	}
}

std::string frame::get_close_msg() const {
	if (get_payload_size() > 2) {
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
		throw "requested payload is over implimentation defined limit";
	}
	
	// limits imposed by the websocket spec
	if (s > BASIC_PAYLOAD_LIMIT && 
		get_opcode() > MAX_FRAME_OPCODE) {
		throw "control frames can't have large payloads";
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
		throw "payload size limit is 63 bits";
	}
	
	m_payload.resize(s);
}

void frame::set_status(uint16_t status,const std::string message) {
	// check for valid statuses
	if (status < 1000 || status > 4999) {
		throw server_error("Status codes must be in the range 1000-4999");
	}
	
	if (status == 1005 || status == 1006) {
		throw server_error("Status codes 1005 and 1006 are reserved for internal use and cannot be written to a frame.");
	}
	
	m_payload.resize(2+message.size());
	
	char val[2];
	
	*reinterpret_cast<uint16_t*>(&val[0]) = htons(status);
	
	m_payload[0] = val[0];
	m_payload[1] = val[1];
	
	std::copy(message.begin(),message.end(),m_payload.begin()+2);
}

void frame::print_frame() const {
	/*unsigned int len = get_header_len();
	
	std::cout << "frame: ";
	// print header
	for (unsigned int i = 0; i < len; i++) {
		std::cout << std::hex << (unsigned short)m_header[i] << " ";
	}
	// print message
	for (auto &i: m_payload) {
		std::cout << i;
	}
	
	std::cout << std::endl;*/
}

unsigned int frame::process_basic_header() {
	m_extended_header_bytes_needed = 0;
	m_payload.empty();
	
	m_extended_header_bytes_needed = get_header_len() - BASIC_HEADER_LENGTH;
	
	return m_extended_header_bytes_needed;
}

void frame::process_extended_header() {
	m_extended_header_bytes_needed = 0;
	
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
		
		mask_index += 2;
	} else if (s == BASIC_PAYLOAD_64BIT_CODE) {
		// reinterpret the second eight bytes as a 16 bit integer in 
		// network byte order. Convert to host byte order and store.
		payload_size = ntohll(*(
			reinterpret_cast<uint64_t*>(&m_header[BASIC_HEADER_LENGTH])
		));
		
		mask_index += 8;
	} else {
		// shouldn't be here
		throw "invalid get_basic_size in process_extended_header";
	}
	
	if (payload_size < s) {
		throw "payload size error";
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
		throw "got frame with payload greater than maximum frame buffer size.";
	}
	m_payload.resize(payload_size);
}

void frame::process_payload() {
	// unmask payload one byte at a time
	for (uint64_t i = 0; i < m_payload.size(); i++) {
		m_payload[i] = (m_payload[i] ^ m_masking_key[i%4]);
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

bool frame::validate_basic_header() const {
	// check for control frame size
	if (get_basic_size() > BASIC_PAYLOAD_LIMIT && is_control()) {
		return false;	
	}
	
	// check for reserved opcodes
	if (get_rsv1() || get_rsv2() || get_rsv3()) {
		return false;
	}
	
	// check for reserved opcodes
	opcode op = get_opcode();
	if (op > 0x02 && op < 0x08) {
		return false;
	}
	if (op > 0x0A) {
		return false;
	}
	
	// check for fragmented control message
	if (is_control() && !get_fin()) {
		return false;
	}
	
	return true;
}

void frame::generate_masking_key() {
	throw "masking key generation not implimented";
	/* TODO: test and tune
	
	boost::random::random_device rng;
	boost::random::uniform_int_distribution<> mask(0,UINT32_MAX);
	
	*(reinterpret_cast<uint32_t *>(m_masking_key)) = mask(rng);
	
	*/
}

void frame::clear_masking_key() {
	m_masking_key[0] = 0;
	m_masking_key[1] = 0;
	m_masking_key[2] = 0;
	m_masking_key[3] = 0;
}