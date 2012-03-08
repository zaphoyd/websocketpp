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

#include "hybi_header.hpp"

#include <cstring>

using websocketpp::processor::hybi_header;

hybi_header::hybi_header() {
    reset();
}
void hybi_header::reset() {
    memset(m_header, 0x00, MAX_HEADER_LENGTH);
    m_state = STATE_BASIC_HEADER;
    m_bytes_needed = BASIC_HEADER_LENGTH;
}

// Writing interface (parse a byte stream)
void hybi_header::consume(std::istream& input) {
    switch (m_state) {
        case STATE_BASIC_HEADER:
            input.read(&m_header[BASIC_HEADER_LENGTH-m_bytes_needed],
                       m_bytes_needed);
            
            m_bytes_needed -= input.gcount();
            
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
            input.read(&m_header[get_header_len()-m_bytes_needed],
                       m_bytes_needed);
            
            m_bytes_needed -= input.gcount();
            
            if (m_bytes_needed == 0) {
                process_extended_header();
                m_state = STATE_READY;
            }
            break;
        default:
            break;
    }
    //std::cout << "header so far: " << zsutil::to_hex(std::string(m_header,MAX_HEADER_LENGTH)) << std::endl;
}
uint64_t hybi_header::get_bytes_needed() const {
    return m_bytes_needed;
}
bool hybi_header::ready() const {
    return m_state == STATE_READY;
}

// Writing interface (set fields directly)
void hybi_header::set_fin(bool fin) {
    set_header_bit(BPB0_FIN,0,fin);
}
void hybi_header::set_rsv1(bool b) {
    set_header_bit(BPB0_RSV1,0,b);
}
void hybi_header::set_rsv2(bool b) {
    set_header_bit(BPB0_RSV2,0,b);
}
void hybi_header::set_rsv3(bool b) {
    set_header_bit(BPB0_RSV3,0,b);
}
void hybi_header::set_opcode(websocketpp::frame::opcode::value op) {
    m_header[0] &= (0xFF ^ BPB0_OPCODE); // clear op bits
    m_header[0] |= op; // set op bits
}
void hybi_header::set_masked(bool masked,int32_t key) {
    if (masked) {
        m_header[1] |= BPB1_MASK;
        set_masking_key(key);
    } else {
        m_header[1] &= (0xFF ^ BPB1_MASK);
        clear_masking_key();
    }
}
void hybi_header::set_payload_size(uint64_t size) {
    if (size <= frame::limits::PAYLOAD_SIZE_BASIC) {
        m_header[1] |= size;
        m_payload_size = size;
    } else if (size <= frame::limits::PAYLOAD_SIZE_EXTENDED) {
        if (get_masked()) {
            // shift mask bytes to the correct position given the new size
            unsigned int mask_offset = get_header_len()-4;
            m_header[1] |= BASIC_PAYLOAD_16BIT_CODE;
            memcpy(&m_header[get_header_len()-4], &m_header[mask_offset], 4);
        } else {
            m_header[1] |= BASIC_PAYLOAD_16BIT_CODE;
        }
        m_payload_size = size;
        *(reinterpret_cast<uint16_t*>(&m_header[BASIC_HEADER_LENGTH])) = htons(static_cast<uint16_t>(size));
    } else if (size <= frame::limits::PAYLOAD_SIZE_JUMBO) {
        if (get_masked()) {
            // shift mask bytes to the correct position given the new size
            unsigned int mask_offset = get_header_len()-4;
            m_header[1] |= BASIC_PAYLOAD_64BIT_CODE;
            memcpy(&m_header[get_header_len()-4], &m_header[mask_offset], 4);
        } else {
            m_header[1] |= BASIC_PAYLOAD_64BIT_CODE;
        }
        m_payload_size = size;
        *(reinterpret_cast<uint64_t*>(&m_header[BASIC_HEADER_LENGTH])) = zsutil::htonll(size);
    } else {
        throw processor::exception("set_payload_size called with value that was too large (>2^63)",processor::error::MESSAGE_TOO_BIG);
    }
    
}
void hybi_header::complete() {
    validate_basic_header();
    m_state = STATE_READY;
}

// Reading interface (get string of bytes)
std::string hybi_header::get_header_bytes() const {
    return std::string(m_header,get_header_len());
}

// Reading interface (get fields directly)
bool hybi_header::get_fin() const {
    return ((m_header[0] & BPB0_FIN) == BPB0_FIN);
}
bool hybi_header::get_rsv1() const {
    return ((m_header[0] & BPB0_RSV1) == BPB0_RSV1);
}
bool hybi_header::get_rsv2() const {
    return ((m_header[0] & BPB0_RSV2) == BPB0_RSV2);
}
bool hybi_header::get_rsv3() const {
    return ((m_header[0] & BPB0_RSV3) == BPB0_RSV3);
}
websocketpp::frame::opcode::value hybi_header::get_opcode() const {
    return frame::opcode::value(m_header[0] & BPB0_OPCODE);
}
bool hybi_header::get_masked() const {
    return ((m_header[1] & BPB1_MASK) == BPB1_MASK);
}
int32_t hybi_header::get_masking_key() const {
    if (!get_masked()) {
        return 0;
    }
    return *reinterpret_cast<const int32_t*>(&m_header[get_header_len()-4]);
}
uint64_t hybi_header::get_payload_size() const {
    return m_payload_size;
}

bool hybi_header::is_control() const {
    return (frame::opcode::is_control(get_opcode()));
}

// private
unsigned int hybi_header::get_header_len() const {
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

uint8_t hybi_header::get_basic_size() const {
    return m_header[1] & BPB1_PAYLOAD;
}

void hybi_header::validate_basic_header() const {    
    // check for control frame size
    if (is_control() && get_basic_size() > frame::limits::PAYLOAD_SIZE_BASIC) {
        throw processor::exception("Control Frame is too large",processor::error::PROTOCOL_VIOLATION);
    }
    
    // check for reserved bits
    if (get_rsv1() || get_rsv2() || get_rsv3()) {
        throw processor::exception("Reserved bit used",processor::error::PROTOCOL_VIOLATION);
    }
    
    // check for reserved opcodes
    if (frame::opcode::reserved(get_opcode())) {
        throw processor::exception("Reserved opcode used",processor::error::PROTOCOL_VIOLATION);
    }
    
    // check for invalid opcodes
    if (frame::opcode::invalid(get_opcode())) {
        throw processor::exception("Invalid opcode used",processor::error::PROTOCOL_VIOLATION);
    }
    
    // check for fragmented control message
    if (is_control() && !get_fin()) {
        throw processor::exception("Fragmented control message",processor::error::PROTOCOL_VIOLATION);
    }
}

void hybi_header::process_basic_header() {
    m_bytes_needed = get_header_len() - BASIC_HEADER_LENGTH;
}
void hybi_header::process_extended_header() {
    uint8_t s = get_basic_size();
    
    if (s <= frame::limits::PAYLOAD_SIZE_BASIC) {
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
        m_payload_size = zsutil::ntohll(*(
            reinterpret_cast<uint64_t*>(&m_header[BASIC_HEADER_LENGTH])
        ));
        
        if (m_payload_size <= frame::limits::PAYLOAD_SIZE_EXTENDED) {
            throw processor::exception("payload length not minimally encoded",
                                       processor::error::PROTOCOL_VIOLATION);
        }
        
    } else {
        // TODO: shouldn't be here how to handle?
        throw processor::exception("invalid get_basic_size in process_extended_header");
    }
}

void hybi_header::set_header_bit(uint8_t bit,int byte,bool value) {
    if (value) {
        m_header[byte] |= bit;
    } else {
        m_header[byte] &= (0xFF ^ bit);
    }
}

void hybi_header::set_masking_key(int32_t key) {
    *(reinterpret_cast<int32_t *>(&m_header[get_header_len()-4])) = key;
}
void hybi_header::clear_masking_key() {
    // this is a no-op as clearing the mask bit also changes the get_header_len
    // method to not include these byte ranges. Whenever the masking bit is re-
    // set a new key is generated anyways.
}
