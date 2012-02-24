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

#include "../processors/processor.hpp"
#include "../websocket_frame.hpp"
#include "../utf8_validator/utf8_validator.hpp"

namespace websocketpp {
namespace message {

class control {
public:
    control() {
        m_payload.reserve(PAYLOAD_SIZE_INIT);
    }
    
    frame::opcode::value get_opcode() const {
        return m_opcode;
    };
    
    const std::string& get_payload() const {
        return m_payload;
    }
    
    uint64_t process_payload(std::istream& input,uint64_t size) {
        char c;
        const uint64_t new_size = m_payload.size() + size;
        uint64_t i;
        
        if (new_size > PAYLOAD_SIZE_MAX) {
            throw processor::exception("Message payload was too large.",processor::error::MESSAGE_TOO_BIG);
        }
        
        i = 0;
        while(input.good() && i < size) {
            c = input.get();
            
            if (!input.fail()) {
                if (m_masking_index >= 0) {
                    c = c ^ m_masking_key.c[(m_masking_index++)%4];
                }
                
                m_payload.push_back(c);
                i++;
            }
            
            if (input.bad()) {
                throw processor::exception("istream read error 2",
                                           processor::error::FATAL_ERROR);
            }
        }
        
        // successfully read all bytes
        return i;
    }
    
    void complete() {
        if (m_opcode == frame::opcode::CLOSE) {
            if (m_payload.size() == 1) {
                throw processor::exception("Single byte close code",processor::error::PROTOCOL_VIOLATION);
            } else if (m_payload.size() >= 2) {
                close::status::value code = close::status::value(get_raw_close_code());
                
                if (close::status::invalid(code)) {
                    throw processor::exception("Close code is not allowed on the wire.",processor::error::PROTOCOL_VIOLATION);
                } else if (close::status::reserved(code)) {
                    throw processor::exception("Close code is reserved.",processor::error::PROTOCOL_VIOLATION);
                }
                
            }
            if (m_payload.size() > 2) {
                if (!m_validator.decode(m_payload.begin()+2,m_payload.end())) {
                    throw processor::exception("Invalid UTF8",processor::error::PAYLOAD_VIOLATION);
                }
                if (!m_validator.complete()) {
                    throw processor::exception("Invalid UTF8",processor::error::PAYLOAD_VIOLATION);
                }
            }
        }
    }
    
    void reset(frame::opcode::value opcode, uint32_t masking_key) {
        m_opcode = opcode;
        set_masking_key(masking_key);
        m_payload.resize(0);
        m_validator.reset();
    }
    
    close::status::value get_close_code() const {
        if (m_payload.size() == 0) {
            return close::status::NO_STATUS;
        } else {
            return close::status::value(get_raw_close_code());
        }
    }
    
    std::string get_close_reason() const {
        if (m_payload.size() > 2) {
            return m_payload.substr(2);
        } else {
            return std::string();
        }
    }
    
    void set_masking_key(int32_t key) {
        //*reinterpret_cast<int32_t*>(m_masking_key) = key;
        m_masking_key.i = key;
        m_masking_index = (key == 0 ? -1 : 0);
    }
private:
    uint16_t get_raw_close_code() const {
        if (m_payload.size() <= 1) {
            throw processor::exception("get_raw_close_code called with invalid size",processor::error::FATAL_ERROR);
        }
        
        union {uint16_t i;char c[2];} val;
         
        val.c[0] = m_payload[0];
        val.c[1] = m_payload[1];
         
        return ntohs(val.i);
    }
    
    static const uint64_t PAYLOAD_SIZE_INIT = 128; // 128B
    static const uint64_t PAYLOAD_SIZE_MAX = 128; // 128B
    
    union masking_key {
        int32_t i;
        char    c[4];
    };
    
    // Message state
    frame::opcode::value        m_opcode;
    
    // UTF8 validation state
    utf8_validator::validator   m_validator;
    
    // Masking state
    masking_key                 m_masking_key;
    //unsigned char               m_masking_key[4];
    int                         m_masking_index;
    
    // Message payload
    std::string                 m_payload;
};

typedef boost::shared_ptr<control> control_ptr;

} // namespace message
} // namespace websocketpp

#endif // WEBSOCKET_CONTROL_MESSAGE_HPP
