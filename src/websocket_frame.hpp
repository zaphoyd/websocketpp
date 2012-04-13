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

#include "common.hpp"



#include "network_utilities.hpp"
#include "processors/processor.hpp"
#include "utf8_validator/utf8_validator.hpp"

#include <boost/utility.hpp>

#if defined(WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include <vector>
#include <cstring>
#include <iostream>
#include <algorithm>

namespace websocketpp {
namespace frame {
    
/* policies to abstract out
 
 - random number generation
 - utf8 validation
 
rng
 int32_t gen()
 
 
class boost_random {
public:
    boost_random()
    int32_t gen();
private:
 boost::random::random_device m_rng;
 boost::random::variate_generator<boost::random::random_device&,boost::random::uniform_int_distribution<> > m_gen;
 }
 
 */
    
template <class rng_policy>
class parser : boost::noncopyable {
public:
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
    static const uint8_t extended_header_length = 12;
    static const uint64_t max_payload_size = 100000000; // 100MB
    
    // create an empty frame for writing into
    parser(rng_policy& rng) : m_rng(rng) {
        reset();
    }
    
    bool ready() const {
        return (m_state == STATE_READY);
    }
    uint64_t get_bytes_needed() const {
        return m_bytes_needed;
    }
    void reset() {
        m_state = STATE_BASIC_HEADER;
        m_bytes_needed = BASIC_HEADER_LENGTH;
        m_degraded = false;
        m_payload.clear();
        std::fill(m_header,m_header+MAX_HEADER_LENGTH,0);
    }
    
    // Method invariant: One of the following must always be true even in the case 
    // of exceptions.
    // - m_bytes_needed > 0
    // - m-state = STATE_READY
    void consume(std::istream &s) {
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
        } catch (const processor::exception& e) {
            // After this point all non-close frames must be considered garbage, 
            // including the current one. Reset it and put the reading frame into
            // a recovery state.
            if (m_degraded == true) {
                throw processor::exception("An error occurred while trying to gracefully recover from a less serious frame error.",processor::error::FATAL_ERROR);
            } else {
                reset();
                m_state = STATE_RECOVERY;
                m_degraded = true;
                
                throw e;
            }
        }
    }
    
    // get pointers to underlying buffers
    char* get_header() {
        return m_header;
    }
    char* get_extended_header() {
        return m_header+BASIC_HEADER_LENGTH;
    }
    unsigned int get_header_len() const {
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
    
    char* get_masking_key() {
        /*if (m_state != STATE_READY) {
            std::cout << "throw" << std::endl;
            throw processor::exception("attempted to get masking_key before reading full header");
        }*/
        return &m_header[get_header_len()-4];
    }
    
    // get and set header bits
    bool get_fin() const {
        return ((m_header[0] & BPB0_FIN) == BPB0_FIN);
    }
    void set_fin(bool fin) {
        if (fin) {
            m_header[0] |= BPB0_FIN;
        } else {
            m_header[0] &= (0xFF ^ BPB0_FIN);
        }
    }
    
    bool get_rsv1() const {
        return ((m_header[0] & BPB0_RSV1) == BPB0_RSV1);
    }
    void set_rsv1(bool b) {
        if (b) {
            m_header[0] |= BPB0_RSV1;
        } else {
            m_header[0] &= (0xFF ^ BPB0_RSV1);
        }
    }
    
    bool get_rsv2() const {
        return ((m_header[0] & BPB0_RSV2) == BPB0_RSV2);
    }
    void set_rsv2(bool b) {
        if (b) {
            m_header[0] |= BPB0_RSV2;
        } else {
            m_header[0] &= (0xFF ^ BPB0_RSV2);
        }
    }
    
    bool get_rsv3() const {
        return ((m_header[0] & BPB0_RSV3) == BPB0_RSV3);
    }
    void set_rsv3(bool b) {
        if (b) {
            m_header[0] |= BPB0_RSV3;
        } else {
            m_header[0] &= (0xFF ^ BPB0_RSV3);
        }
    }
    
    opcode::value get_opcode() const {
        return frame::opcode::value(m_header[0] & BPB0_OPCODE);
    }
    void set_opcode(opcode::value op) {
        if (opcode::reserved(op)) {
            throw processor::exception("reserved opcode",processor::error::PROTOCOL_VIOLATION);
        }
        
        if (opcode::invalid(op)) {
            throw processor::exception("invalid opcode",processor::error::PROTOCOL_VIOLATION);
        }
        
        if (is_control() && get_basic_size() > limits::PAYLOAD_SIZE_BASIC) {
            throw processor::exception("control frames can't have large payloads",processor::error::PROTOCOL_VIOLATION);
        }
        
        m_header[0] &= (0xFF ^ BPB0_OPCODE); // clear op bits
        m_header[0] |= op; // set op bits
    }
    
    bool get_masked() const {
        return ((m_header[1] & BPB1_MASK) == BPB1_MASK);
    }
    void set_masked(bool masked) {
        if (masked) {
            m_header[1] |= BPB1_MASK;
            generate_masking_key();
        } else {
            m_header[1] &= (0xFF ^ BPB1_MASK);
            clear_masking_key();
        }
    }
    
    uint8_t get_basic_size() const {
        return m_header[1] & BPB1_PAYLOAD;
    }
    size_t get_payload_size() const {
        if (m_state != STATE_READY && m_state != STATE_PAYLOAD) {
            // TODO: how to handle errors like this?
            throw "attempted to get payload size before reading full header";
        }
        
        return m_payload.size();
    }
    
    close::status::value get_close_status() const {
        if (get_payload_size() == 0) {
            return close::status::NO_STATUS;
        } else if (get_payload_size() >= 2) {
            char val[2] = { m_payload[0], m_payload[1] };
            uint16_t code;

            std::copy(val,val+sizeof(code),&code);            
            code = ntohs(code);
            
            return close::status::value(code);
        } else {
            return close::status::PROTOCOL_ERROR;
        }
    }
    std::string get_close_msg() const {
        if (get_payload_size() > 2) {
            uint32_t state = utf8_validator::UTF8_ACCEPT;
            uint32_t codep = 0;
            validate_utf8(&state,&codep,2);
            if (state != utf8_validator::UTF8_ACCEPT) {
                throw processor::exception("Invalid UTF-8 Data",processor::error::PAYLOAD_VIOLATION);
            }
            return std::string(m_payload.begin()+2,m_payload.end());
        } else {
            return std::string();
        }
    }
    
    std::vector<unsigned char> &get_payload() {
        return m_payload;
    }
    
    void set_payload(const std::vector<unsigned char>& source) {
        set_payload_helper(source.size());
        
        std::copy(source.begin(),source.end(),m_payload.begin());
    }
    void set_payload(const std::string& source) {
        set_payload_helper(source.size());
        
        std::copy(source.begin(),source.end(),m_payload.begin());
    }
    void set_payload_helper(size_t s) {
        if (s > max_payload_size) {
            throw processor::exception("requested payload is over implementation defined limit",processor::error::MESSAGE_TOO_BIG);
        }
        
        // limits imposed by the websocket spec
        if (is_control() && s > limits::PAYLOAD_SIZE_BASIC) {
            throw processor::exception("control frames can't have large payloads",processor::error::PROTOCOL_VIOLATION);
        }
        
        bool masked = get_masked();
        
        if (s <= limits::PAYLOAD_SIZE_BASIC) {
            m_header[1] = s;
        } else if (s <= limits::PAYLOAD_SIZE_EXTENDED) {
            m_header[1] = BASIC_PAYLOAD_16BIT_CODE;
            
            // this reinterprets the second pair of bytes in m_header as a
            // 16 bit int and writes the payload size there as an integer
            // in network byte order
            *reinterpret_cast<uint16_t*>(&m_header[BASIC_HEADER_LENGTH]) = htons(s);
        } else if (s <= limits::PAYLOAD_SIZE_JUMBO) {
            m_header[1] = BASIC_PAYLOAD_64BIT_CODE;
            *reinterpret_cast<uint64_t*>(&m_header[BASIC_HEADER_LENGTH]) = zsutil::htonll(s);
        } else {
            throw processor::exception("payload size limit is 63 bits",processor::error::PROTOCOL_VIOLATION);
        }
        
        if (masked) {
            m_header[1] |= BPB1_MASK;
        }
        
        m_payload.resize(s);
    }
    
    void set_status(close::status::value status,const std::string message = "") {
        // check for valid statuses
        if (close::status::invalid(status)) {
            std::stringstream err;
            err << "Status code " << status << " is invalid";
            throw processor::exception(err.str());
        }
        
        if (close::status::reserved(status)) {
            std::stringstream err;
            err << "Status code " << status << " is reserved";
            throw processor::exception(err.str());
        }
        
        m_payload.resize(2+message.size());
        
        char val[2];
        
        *reinterpret_cast<uint16_t*>(&val[0]) = htons(status);
        
        bool masked = get_masked();
        
        m_header[1] = message.size()+2;
        
        if (masked) {
            m_header[1] |= BPB1_MASK;
        }
        
        m_payload[0] = val[0];
        m_payload[1] = val[1];
        
        std::copy(message.begin(),message.end(),m_payload.begin()+2);
    }
    
    bool is_control() const {
        return (opcode::is_control(get_opcode()));
    }
    
    std::string print_frame() const {
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
    
    // reads basic header, sets and returns m_header_bits_needed
    void process_basic_header() {
        m_bytes_needed = get_header_len() - BASIC_HEADER_LENGTH;
    }
    void process_extended_header() {
        uint8_t s = get_basic_size();
        uint64_t payload_size;
        int mask_index = BASIC_HEADER_LENGTH;
        
        if (s <= limits::PAYLOAD_SIZE_BASIC) {
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
                throw processor::exception(err.str(),processor::error::PROTOCOL_VIOLATION);
            }
            
            mask_index += 2;
        } else if (s == BASIC_PAYLOAD_64BIT_CODE) {
            // reinterpret the second eight bytes as a 64 bit integer in 
            // network byte order. Convert to host byte order and store.
            payload_size = zsutil::ntohll(*(
                reinterpret_cast<uint64_t*>(&m_header[BASIC_HEADER_LENGTH])
            ));
            
            if (payload_size <= limits::PAYLOAD_SIZE_EXTENDED) {
                m_bytes_needed = payload_size;
                throw processor::exception("payload length not minimally encoded",
                                processor::error::PROTOCOL_VIOLATION);
            }
            
            mask_index += 8;
        } else {
            // TODO: shouldn't be here how to handle?
            throw processor::exception("invalid get_basic_size in process_extended_header");
        }
        
        if (get_masked() == 0) {
            clear_masking_key();
        } else {
            // TODO: this should be removed entirely once it is confirmed to not
            // be used by anything.
            // std::copy(m_header[mask_index],m_header[mask_index+4],m_masking_key);
            /*m_masking_key[0] = m_header[mask_index+0];
            m_masking_key[1] = m_header[mask_index+1];
            m_masking_key[2] = m_header[mask_index+2];
            m_masking_key[3] = m_header[mask_index+3];*/
        }
        
        if (payload_size > max_payload_size) {
            // TODO: frame/message size limits
            // TODO: find a way to throw a server error without coupling frame
            //       with server
            // throw websocketpp::server_error("got frame with payload greater than maximum frame buffer size.");
            throw "Got frame with payload greater than maximum frame buffer size.";
        }
        m_payload.resize(payload_size);
        m_bytes_needed = payload_size;
    }
    
    void process_payload() {
        if (get_masked()) {
            char *masking_key = get_masking_key();
            
            for (uint64_t i = 0; i < m_payload.size(); i++) {
                m_payload[i] = (m_payload[i] ^ masking_key[i%4]);
            }
        }
    }
    
    // experiment with more efficient masking code.
    void process_payload2() {
        // unmask payload one byte at a time
        
        //uint64_t key = (*((uint32_t*)m_masking_key;)) << 32;
        //key += *((uint32_t*)m_masking_key);
        
        // might need to switch byte order
        /*uint32_t key = *((uint32_t*)m_masking_key);
        
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
        }*/
    }
    
    void validate_utf8(uint32_t* state,uint32_t* codep,size_t offset = 0) const {
        for (size_t i = offset; i < m_payload.size(); i++) {
            using utf8_validator::decode;
            
            if (decode(state,codep,m_payload[i]) == utf8_validator::UTF8_REJECT) {
                throw processor::exception("Invalid UTF-8 Data",processor::error::PAYLOAD_VIOLATION);
            }
        }
    }
    void validate_basic_header() const {
        // check for control frame size
        if (is_control() && get_basic_size() > limits::PAYLOAD_SIZE_BASIC) {
            throw processor::exception("Control Frame is too large",processor::error::PROTOCOL_VIOLATION);
        }
        
        // check for reserved bits
        if (get_rsv1() || get_rsv2() || get_rsv3()) {
            throw processor::exception("Reserved bit used",processor::error::PROTOCOL_VIOLATION);
        }
        
        // check for reserved opcodes
        if (opcode::reserved(get_opcode())) {
            throw processor::exception("Reserved opcode used",processor::error::PROTOCOL_VIOLATION);
        }
        
        // check for fragmented control message
        if (is_control() && !get_fin()) {
            throw processor::exception("Fragmented control message",processor::error::PROTOCOL_VIOLATION);
        }
    }
    
    void generate_masking_key() {
        *(reinterpret_cast<int32_t *>(&m_header[get_header_len()-4])) = m_rng.rand();
    }
    void clear_masking_key() {
        // this is a no-op as clearing the mask bit also changes the get_header_len
        // method to not include these byte ranges. Whenever the masking bit is re-
        // set a new key is generated anyways.
    }
    
private:
    static const uint8_t STATE_BASIC_HEADER = 1;
    static const uint8_t STATE_EXTENDED_HEADER = 2;
    static const uint8_t STATE_PAYLOAD = 3;
    static const uint8_t STATE_READY = 4;
    static const uint8_t STATE_RECOVERY = 5;
    
    uint8_t     m_state;
    uint64_t    m_bytes_needed;
    bool        m_degraded;
    
    char m_header[MAX_HEADER_LENGTH];
    std::vector<unsigned char> m_payload;
        
    rng_policy& m_rng;
};

}
}

#endif // WEBSOCKET_FRAME_HPP
