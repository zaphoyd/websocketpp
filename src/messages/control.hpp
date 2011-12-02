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

#ifndef WEBSOCKET_CONTROL_MESSAGE_HPP
#define WEBSOCKET_CONTROL_MESSAGE_HPP

#include <iostream>
#include <boost/shared_ptr.hpp>

#include "../websocket_frame.hpp"
#include "../utf8_validator/utf8_validator.hpp"

namespace websocketpp {
namespace message {

class control {
public:
    control() {
        m_payload.reserve(SIZE_INIT);
    }
    
	frame::opcode::value get_opcode() const {
        return m_opcode;
    };
	
    uint64_t process_payload(std::istream& input,uint64_t size) {
        char c;
        char *masking_key = reinterpret_cast<char *>(&m_masking_key);
        const uint64_t new_size = m_payload.size() + size;
        uint64_t i;
		
        if (new_size > SIZE_MAX) {
            // TODO: real exception
            throw "message too big exception";
        }
        
        for (i = 0; i < size; ++i) {
            if (input.good()) {
               c = input.get(); 
            } else if (input.eof()) {
                break;
            } else {
                // istream read error? throw?
                throw "TODO: fix";
            }
            if (input.good()) {
                // process c
                if (m_masking_key) {
                    c = c ^ masking_key[(m_masking_index++)%4];
                }
				
				/*if (m_opcode == frame::opcode::TEXT && !m_validator.consume(static_cast<uint32_t>(c))) {
                    // TODO
                    throw "bad utf8";
                }*/
				
                // add c to payload 
                m_payload.push_back(c);
            } else if (input.eof()) {
                break;
            } else {
                // istream read error? throw?
                throw "TODO: fix";
            }
        }
        
        // successfully read all bytes
        return i;
    }
    
    void reset(frame::opcode::value opcode, uint32_t masking_key) {
        m_opcode = opcode;
        m_masking_key = masking_key;
        m_masking_index = 0;
        m_payload.resize(0);
		m_validator.reset();
    }
private:
    static const uint64_t SIZE_INIT = 128; // 128B
    static const uint64_t SIZE_MAX = 128; // 128B
    
	frame::opcode::value		m_opcode;
    utf8_validator::validator	m_validator;
    uint32_t                    m_masking_key;
    int                         m_masking_index;
    std::string					m_payload;
};

typedef boost::shared_ptr<control> control_ptr;
	
} // namespace message
} // namespace websocketpp

#endif // WEBSOCKET_CONTROL_MESSAGE_HPP
