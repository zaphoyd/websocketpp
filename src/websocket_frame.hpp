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

#ifndef WEBSOCKET_FRAME_HPP
#define WEBSOCKET_FRAME_HPP

#include "network_utilities.hpp"

#include <string>
#include <vector>
#include <cstring>

#include <boost/random.hpp>
#include <boost/random/random_device.hpp>

namespace websocketpp {

/* policies to abstract out
 
 - random number generation
 - utf8 validation
 
 */	

class frame {
public:	
	enum opcode_s {
		CONTINUATION_FRAME = 0x00,
		TEXT_FRAME = 0x01,
		BINARY_FRAME = 0x02,
		CONNECTION_CLOSE = 0x08,
		PING = 0x09,
		PONG = 0x0A
	};
	
	typedef enum opcode_s opcode;
	
	static const uint8_t MAX_FRAME_OPCODE = 0x07;
	
	static const uint8_t STATE_BASIC_HEADER = 1;
	static const uint8_t STATE_EXTENDED_HEADER = 2;
	static const uint8_t STATE_PAYLOAD = 3;
	static const uint8_t STATE_READY = 4;
	static const uint8_t STATE_RECOVERY = 5;
	
	static const uint16_t FERR_FATAL_SESSION_ERROR = 0; // force session end
	static const uint16_t FERR_SOFT_SESSION_ERROR = 1; // should log and ignore
	static const uint16_t FERR_PROTOCOL_VIOLATION = 2; // must end session
	static const uint16_t FERR_PAYLOAD_VIOLATION = 3; // should end session
	static const uint16_t FERR_INTERNAL_SERVER_ERROR = 4; // cleanly end session
	static const uint16_t FERR_MSG_TOO_BIG = 5;
	
	// basic payload byte flags
	static const uint8_t BPB0_OPCODE = 0x0F;
	static const uint8_t BPB0_RSV3 = 0x10;
	static const uint8_t BPB0_RSV2 = 0x20;
	static const uint8_t BPB0_RSV1 = 0x40;
	static const uint8_t BPB0_FIN = 0x80;
	static const uint8_t BPB1_PAYLOAD = 0x7F;
	static const uint8_t BPB1_MASK = 0x80;
	
	static const uint8_t BASIC_PAYLOAD_LIMIT = 0x7D; // 125
	static const uint8_t BASIC_PAYLOAD_16BIT_CODE = 0x7E; // 126
	static const uint16_t PAYLOAD_16BIT_LIMIT = 0xFFFF; // 2^16, 65535 
	static const uint8_t BASIC_PAYLOAD_64BIT_CODE = 0x7F; // 127
	static const uint64_t PAYLOAD_64BIT_LIMIT = 0x7FFFFFFFFFFFFFFF; // 2^63
	
	static const unsigned int BASIC_HEADER_LENGTH = 2;		
	static const unsigned int MAX_HEADER_LENGTH = 14;
	static const uint8_t extended_header_length = 12;
	static const uint64_t max_payload_size = 100000000; // 100MB
	
	// create an empty frame for writing into
	frame() : m_gen(m_rng, 
	          boost::random::uniform_int_distribution<>(INT32_MIN,INT32_MAX)),m_degraded(false) {
		reset();
	}
	
	uint8_t get_state() const;
	uint64_t get_bytes_needed() const;
	void reset();
	
	void consume(std::istream &s);
	
	// get pointers to underlying buffers
	char* get_header();
	char* get_extended_header();
	unsigned int get_header_len() const;
	
	char* get_masking_key();
	
	// get and set header bits
	bool get_fin() const;
	void set_fin(bool fin);
	
	bool get_rsv1() const;
	void set_rsv1(bool b);
	
	bool get_rsv2() const;
	void set_rsv2(bool b);
	
	bool get_rsv3() const;
	void set_rsv3(bool b);
	
	opcode get_opcode() const;
	void set_opcode(opcode op);
	
	bool get_masked() const;
	void set_masked(bool masked);
	
	uint8_t get_basic_size() const;
	size_t get_payload_size() const;
	
	uint16_t get_close_status() const;
	std::string get_close_msg() const;
	
	std::vector<unsigned char> &get_payload();
	
	void set_payload(const std::vector<unsigned char> source);
	void set_payload(const std::string source);
	void set_payload_helper(size_t s);
	
	void set_status(uint16_t status,const std::string message = "");
	
	bool is_control() const;
	
	std::string print_frame() const;
	
	// reads basic header, sets and returns m_header_bits_needed
	void process_basic_header();
	void process_extended_header();
	void process_payload();
	void process_payload2(); // experiment with more efficient masking code.
	
	void validate_utf8(uint32_t* state,uint32_t* codep,size_t offset = 0) const;
	void validate_basic_header() const;
	
	void generate_masking_key();
	void clear_masking_key();
	
private:
	uint8_t		m_state;
	uint64_t	m_bytes_needed;
	bool		m_degraded;
	
	char m_header[MAX_HEADER_LENGTH];
	std::vector<unsigned char> m_payload;
	
	char m_masking_key[4];	
	
	boost::random::random_device m_rng;
	boost::random::variate_generator<boost::random::random_device&, 
	                                 boost::random::uniform_int_distribution<> > 
	    m_gen;
};

// Exception classes
class frame_error : public std::exception {
public:	
	frame_error(const std::string& msg,
				uint16_t code = frame::FERR_FATAL_SESSION_ERROR) 
	 : m_msg(msg),m_code(code) {}
	~frame_error() throw() {}
	
	virtual const char* what() const throw() {
		return m_msg.c_str();
	}
	
	uint16_t code() const throw() {
		return m_code;
	}
	
	std::string m_msg;
	uint16_t m_code;
};
	
}

#endif // WEBSOCKET_FRAME_HPP
