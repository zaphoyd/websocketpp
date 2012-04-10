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

#ifndef WEBSOCKET_HYBI_UTIL_HPP
#define WEBSOCKET_HYBI_UTIL_HPP

#include "../common.hpp"

namespace websocketpp {
namespace processor {
namespace hybi_util {    

// type used to store a masking key
union masking_key_type {
    int32_t i;
    char    c[4];
};

// extract a masking key into a value the size of a machine word. Machine word
// size must be 4 or 8
size_t prepare_masking_key(const masking_key_type& key);

// circularly shifts the supplied prepared masking key by offset bytes
// prepared_key must be the output of prepare_masking_key with the associated
//    restrictions on the machine word size.
// offset must be 0, 1, 2, or 3
size_t circshift_prepared_key(size_t prepared_key, size_t offset);

// basic byte by byte mask
template <typename iter_type>
void byte_mask(iter_type b, iter_type e, const masking_key_type& key, size_t key_offset = 0) {
    size_t key_index = key_offset;
    for (iter_type i = b; i != e; i++) {
        *i ^= key.c[key_index++];
        key_index %= 4;
    }
}

// exactly masks the bytes from start to end using key `key`
void word_mask_exact(char* data,size_t length,const masking_key_type& key);

} // namespace hybi_util
} // namespace processor
} // namespace websocketpp

#endif // WEBSOCKET_HYBI_UTIL_HPP
