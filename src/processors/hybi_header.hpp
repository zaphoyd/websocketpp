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

#ifndef WEBSOCKET_PROCESSOR_HYBI_HEADER_HPP
#define WEBSOCKET_PROCESSOR_HYBI_HEADER_HPP

#include "processor.hpp"
#include "../websocket_frame.hpp" // TODO: remove this dependency

namespace websocketpp {
namespace processor {

class hybi_header {
public:
	hybi_header();
	
	uint64_t get_bytes_needed() const;
	
	void reset();
	
	bool ready() const;
	
	void consume(std::istream& input);
	
	unsigned int get_header_len() const;
	
	int32_t get_masking_key();
	
	// get and set header bits
	bool get_fin() const;
	void set_fin(bool fin);
	
	bool get_rsv1() const;
	void set_rsv1(bool b);
	
	bool get_rsv2() const;
	void set_rsv2(bool b);
	
	bool get_rsv3() const;
	void set_rsv3(bool b);
	
	frame::opcode::value get_opcode() const;
	void set_opcode(frame::opcode::value op);
	
	bool get_masked() const;
	void set_masked(bool masked,int32_t key);
	
	uint8_t get_basic_size() const;
	size_t get_payload_size() const;
	
    void set_payload_size(size_t size);
    
	bool is_control() const;
	
	void process_basic_header();
	void process_extended_header();
	
	void validate_basic_header() const;
	
	void set_masking_key(int32_t key);
	void clear_masking_key();
	
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

#endif // WEBSOCKET_PROCESSOR_HYBI_HEADER_HPP
