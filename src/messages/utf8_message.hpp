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

#ifndef WEBSOCKET_UTF8_MESSAGE_HPP
#define WEBSOCKET_UTF8_MESSAGE_HPP

namespace websocketpp {
namespace message {

class utf8_message : public basic_message {
public:
    utf8_message() {
        m_payload.reserve(SIZE_INIT);
    }
    
    uint64_t process_payload(std::istream& input,uint64_t size) {
        char c;
        
        const uint64_t new_size = m_payload.size() + size;
        
        if (new_size > SIZE_MAX) {
            // TODO: real exception
            throw "message too big exception";
        }
        
        if (new_size > m_payload.capacity()) {
            m_payload.reserve(max(new_size,2*m_payload.capacity()));
        }
        
        for (uint64_t i = 0; i < size; ++i) {
            if (input.good()) {
                c = input.get(); 
            } else if (input.eof()) {
                break;
            } else {
                // istream read error? throw?
            }
            if (input.good()) {
                // process c
                if (m_masking_key) {
                    c = c ^ m_masking_key[m_masking_index++%4];
                }
                
                if (!m_validator.consume(static_cast<uint32_t>(c))) {
                    // TODO
                    throw "bad utf8";
                }
                
                // add c to payload 
                m_payload.push_back(c);
            } else if (input.eof()) {
                break;
            } else {
                // istream read error? throw?
            }
        }
        
        // we have read all bytes in the message.
        return i;
    }
    
    void complete() {
        if (m_validator.complete()) {
            // TODO
            throw "bad utf8";
        }
    }
    
    void reset(opcode::value opcode, uint32_t masking_key) {
        m_opcode = opcode;
        m_masking_key = masking_key;
        m_masking_index = 0;
        m_payload.resize(0);
        m_validator.reset();
    }
private:
    static const uint64_t SIZE_INIT = 1000000; // 1MB
    static const uint64_t SIZE_MAX = 100000000;// 100MB
    
    uint64_t                    m_max_size;
    utf8_validator::validator	m_validator;
    uint32_t                    m_masking_key;
    int                         m_masking_index;
    std::string                 m_payload;
};
    
} // namespace message
} // namespace websocketpp

#endif // WEBSOCKET_UTF8_MESSAGE_HPP
