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

#include <boost/detail/atomic_count.hpp>

#include <algorithm>
#include <istream>

namespace websocketpp {
namespace message {

/*class intrusive_ptr_base {
public:
    intrusive_ptr_base() : ref_count(0) {}
    intrusive_ptr_base(intrusive_ptr_base const&) : ref_count(0) {}
    intrusive_ptr_base& operator=(intrusive_ptr_base const& rhs) {
        return *this;
    }
    friend void intrusive_ptr_add_ref(intrusive_ptr_base const* s) {
        assert(s->ref_count >= 0);
        assert(s != 0);
        ++s->ref_count;
    }
    friend void intrusive_ptr_release(intrusive_ptr_base const* s) {
        assert(s->ref_count > 0);
        assert(s != 0);
        
        // TODO: thread safety
        long count = --s->ref_count;
        if (count == 1 && endpoint != NULL) {
            // recycle if endpoint exists
            endpoint->recycle();
        } else if (count == 0) {
            boost::checked_delete(static_cast<intrusive_ptr_base const*>(s));
        }
    }
    
    detach() {
        endpoint = NULL;
    }
private:
    websocketpp::endpoint_base* endpoint;
    mutable boost::detail::atomic_count ref_count;
};*/


class data {
public:
    data();
	
    void reset(frame::opcode::value opcode);
    
    frame::opcode::value get_opcode() const;
	const std::string& get_payload() const;
    const std::string& get_header() const;
    
    // ##reading##
    // sets the masking key to be used to unmask as bytes are read.
    void set_masking_key(int32_t key);
    
    // read at most size bytes from a payload stream and perform unmasking/utf8
    // validation. Returns number of bytes read.
    // throws a processor::exception if the message is too big, there is a fatal
    // istream read error, or invalid UTF8 data is read for a text message
    uint64_t process_payload(std::istream& input,uint64_t size);
	void process_character(unsigned char c);
	void complete();
    
    // ##writing##
    // sets the payload to payload. Performs max size and UTF8 validation 
    // immediately and throws processor::exception if it fails
    void set_payload(const std::string& payload);
    void append_payload(const std::string& payload);
    
    void set_header(const std::string& header);
    
    // Performs masking and header generation if it has not been done already.
    void set_prepared(bool b);
    bool get_prepared() const;
    void acquire();
    void release();
    bool done() const;
    void mask();
	
    int m_max_refcount;
private:
    static const uint64_t PAYLOAD_SIZE_INIT = 1000; // 1KB
    static const uint64_t PAYLOAD_SIZE_MAX = 100000000;// 100MB
    
    enum index_value {
        M_MASK_KEY_ZERO = -2,
        M_NOT_MASKED = -1,
        M_BYTE_0 = 0,
        M_BYTE_1 = 1,
        M_BYTE_2 = 2,
        M_BYTE_3 = 3
    };
    
	// Message state
	frame::opcode::value		m_opcode;
	
	// UTF8 validation state
    utf8_validator::validator	m_validator;
	
	// Masking state
	unsigned char				m_masking_key[4];
    // m_masking_index can take on
    index_value                 m_masking_index;
	
	// Message buffers
    int                         m_refcount;
    
    std::string                 m_header;
    std::string					m_payload;
};

typedef boost::shared_ptr<data> data_ptr;
	
} // namespace message
} // namespace websocketpp

#endif // WEBSOCKET_DATA_MESSAGE_HPP