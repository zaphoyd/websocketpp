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

#ifndef WEBSOCKET_DATA_MESSAGE_HPP
#define WEBSOCKET_DATA_MESSAGE_HPP

#include "../common.hpp"
#include "../utf8_validator/utf8_validator.hpp"

#include <algorithm>
#include <istream>

namespace websocketpp {
namespace message {

class data {
public:
    data();
	
    frame::opcode::value get_opcode() const;
	const std::string& get_payload() const;
    
    uint64_t process_payload(std::istream& input,uint64_t size);
	void process_character(unsigned char c);
    void reset(frame::opcode::value opcode);
	void complete();
	void set_masking_key(int32_t key);
private:
    static const uint64_t PAYLOAD_SIZE_INIT = 1000; // 1KB
    static const uint64_t PAYLOAD_SIZE_MAX = 100000000;// 100MB
    
	// Message state
	frame::opcode::value		m_opcode;
	
	// UTF8 validation state
    utf8_validator::validator	m_validator;
	
	// Masking state
	unsigned char				m_masking_key[4];
    int                         m_masking_index;
	
	// Message buffers
    std::string					m_payload;
};

typedef boost::shared_ptr<data> data_ptr;
	
} // namespace message
} // namespace websocketpp

#endif // WEBSOCKET_DATA_MESSAGE_HPP