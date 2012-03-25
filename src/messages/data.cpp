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

#include "data.hpp"

#include "../processors/processor.hpp"
#include "../processors/hybi_header.hpp"

using websocketpp::message::data;

data::data(data::pool_ptr p, size_t s) : m_prepared(false),m_index(s),m_ref_count(0),m_pool(p),m_live(false) {
    m_payload.reserve(PAYLOAD_SIZE_INIT);
}
    
websocketpp::frame::opcode::value data::get_opcode() const {
    return m_opcode;
}

const std::string& data::get_payload() const {
    return m_payload;
}
const std::string& data::get_header() const {
    return m_header;
}

uint64_t data::process_payload(std::istream& input,uint64_t size) {
    unsigned char c;
    const uint64_t new_size = m_payload.size() + size;
    uint64_t i;
    
    if (new_size > PAYLOAD_SIZE_MAX) {
        throw processor::exception("Message too big",processor::error::MESSAGE_TOO_BIG);
    }
    
    if (new_size > m_payload.capacity()) {
        m_payload.reserve(std::max<size_t>(
            static_cast<size_t>(new_size), static_cast<size_t>(2*m_payload.capacity())
        ));
    }
    
    i = 0;
    while(input.good() && i < size) {
        c = input.get();
        
        if (!input.fail()) {
            process_character(c);
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

void data::process_character(unsigned char c) {
    if (m_masking_index >= 0) {
        c = c ^ m_masking_key.c[m_masking_index];
        m_masking_index = index_value((m_masking_index+1)%4);
    }
    
    if (m_opcode == frame::opcode::TEXT && 
        !m_validator.consume(static_cast<uint32_t>((unsigned char)(c))))
    {
        throw processor::exception("Invalid UTF8 data",processor::error::PAYLOAD_VIOLATION);
    }
    
    // add c to payload 
    m_payload.push_back(c);
}
    
void data::reset(websocketpp::frame::opcode::value opcode) {
    m_opcode = opcode;
    m_masking_index = M_NOT_MASKED;
    m_payload.clear();
    m_validator.reset();
    m_prepared = false;
}

void data::complete() {
    if (m_opcode == frame::opcode::TEXT) {
        if (!m_validator.complete()) {
            throw processor::exception("Invalid UTF8 data",processor::error::PAYLOAD_VIOLATION);
        }
    }
    
}

void data::validate_payload() {
    if (m_opcode == frame::opcode::TEXT) {
        if (!m_validator.decode(m_payload.begin(), m_payload.end())) {
            throw exception("Invalid UTF8 data",error::PAYLOAD_VIOLATION);
        }
        
        if (!m_validator.complete()) {
            throw exception("Invalid UTF8 data",error::PAYLOAD_VIOLATION);
        }
    }
}

void data::set_masking_key(int32_t key) {
    m_masking_key.i = key;
    m_masking_index = (key == 0 ? M_MASK_KEY_ZERO : M_BYTE_0); 
}

void data::set_prepared(bool b) {
    m_prepared = b;
}

bool data::get_prepared() const {
    return m_prepared; 
}

// This could be further optimized using methods that write directly into the
// m_payload buffer
void data::set_payload(const std::string& payload) {
    m_payload.reserve(payload.size());
    m_payload = payload;
}
void data::append_payload(const std::string& payload) {
    m_payload.reserve(m_payload.size()+payload.size());
    m_payload.append(payload);
}
void data::mask() {
    if (m_masking_index >= 0) {
        // By default WebSocket++ performs block masking/unmasking in a mannor that makes
        // some assumptions about the nature of the machine and STL library used. In 
        // particular the assumption is either a 32 or 64 bit word size and an STL with
        // std::string::data returning a contiguous char array.
        //
        // This method improves performance by 3-8x depending on the ratio of small to 
        // large messages and the availability of a 64 bit processor.
        //
        // To disable this optimization (for use with alternative STL implementations or
        // processors) define WEBSOCKETPP_STRICT_MASKING when compiling the library. This
        // will force the library to perform masking in single byte chunks.
        #define WEBSOCKETPP_STRICT_MASKING
        
        #ifndef WEBSOCKETPP_STRICT_MASKING
            size_t size = m_payload.size()/sizeof(size_t);
            size_t key = m_masking_key.i;
            if (sizeof(size_t) == 8) {
                key <<= 32;
                key |= (static_cast<size_t>(m_masking_key.i) & 0x00000000FFFFFFFFLL);
            }
            size_t* data = reinterpret_cast<size_t*>(const_cast<char*>(m_payload.data()));
            for (size_t i = 0; i < size; i++) {
                data[i] ^= key;
            }
            for (size_t i = size*sizeof(size_t); i < m_payload.size(); i++) {
                m_payload[i] ^= m_masking_key.c[i%4];
            }
        #else
            size_t len = m_payload.size();
            for (size_t i = 0; i < len; i++) {
                m_payload[i] ^= m_masking_key.c[i%4];
            }
            /*for (std::string::iterator it = m_payload.begin(); it != m_payload.end(); it++) {
                (*it) = *it ^ m_masking_key.c[m_masking_index];
                m_masking_index = index_value((m_masking_index+1)&3);
            }*/
        #endif
    }
}

void data::set_header(const std::string& header) {
    m_header = header;
}





//
void data::set_live() {
    m_live = true;
}
size_t data::get_index() const {
    return m_index;
}

