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

namespace websocketpp {
namespace processor {

/// Describes a processor for reading and writing WebSocket frame headers
/**
 * The hybi_header class provides a processor capable of reading and writing
 * WebSocket frame headers. It has two writing modes and two reading modes.
 * 
 * Writing method 1: call consume() until ready()
 * Writing method 2: call set_* methods followed by complete()
 * 
 * Writing methods are valid only when ready() returns false. Use reset() to 
 * reset the header for writing again. Mixing writing methods between calls to
 * reset() may behave unpredictably.
 * 
 * Reading method 1: call get_header_bytes() to return a string of bytes
 * Reading method 2: call get_* methods to read individual values
 * 
 * Reading methods are valid only when ready() is true.
 * 
 * @par Thread Safety
 * @e Distinct @e objects: Safe.@n
 * @e Shared @e objects: Unsafe
 */
class hybi_header {
public:
    /// Construct a header processor and initialize for writing
    hybi_header();
    /// Reset a header processor for writing
    void reset();
    
    // Writing interface (parse a byte stream)
    // valid only if ready() returns false
    // Consume will throw a processor::exception in the case that the bytes it
    // read do not form a valid WebSocket frame header.
    void consume(std::istream& input);
    uint64_t get_bytes_needed() const;
    bool ready() const;
    
    // Writing interface (set fields directly)
    // valid only if ready() returns false
    // set_* may allow invalid values. Call complete() once values are set to
    // check for header validity.
    void set_fin(bool fin);
    void set_rsv1(bool b);
    void set_rsv2(bool b);
    void set_rsv3(bool b);
    void set_opcode(websocketpp::frame::opcode::value op);
    void set_masked(bool masked,int32_t key);
    void set_payload_size(uint64_t size);
    // Complete will throw a processor::exception in the case that the 
    // combination of values set do not form a valid WebSocket frame header.
    void complete();
    
    // Reading interface (get string of bytes)
    // valid only if ready() returns true
    std::string get_header_bytes() const;
    
    // Reading interface (get fields directly)
    // valid only if ready() returns true
    bool get_fin() const;
    bool get_rsv1() const;
    bool get_rsv2() const;
    bool get_rsv3() const;
    frame::opcode::value get_opcode() const;
    bool get_masked() const;
    // will return zero in the case where get_masked() is false. Note:
    // a masking key of zero is slightly different than no mask at all.
    int32_t get_masking_key() const;
    uint64_t get_payload_size() const;
    
    bool is_control() const;
private:
    // general helper functions
    unsigned int get_header_len() const;
    uint8_t get_basic_size() const;
    void validate_basic_header() const;
    
    // helper functions for writing
    void process_basic_header();
    void process_extended_header();
    void set_header_bit(uint8_t bit,int byte,bool value);
    void set_masking_key(int32_t key);
    void clear_masking_key();
    
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
    static const uint8_t STATE_WRITE = 4;
    
    uint8_t     m_state;
    std::streamsize m_bytes_needed;
    uint64_t    m_payload_size;
    char m_header[MAX_HEADER_LENGTH];
};
    
} // namespace processor
} // namespace websocketpp

#endif // WEBSOCKET_PROCESSOR_HYBI_HEADER_HPP
