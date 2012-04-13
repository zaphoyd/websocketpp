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

#include "hybi_util.hpp"

namespace websocketpp {
namespace processor {
namespace hybi_util { 

size_t prepare_masking_key(const masking_key_type& key) {
    size_t prepared_key = key.i;
	size_t wordSize = sizeof(size_t);
    if (wordSize == 8) {
        prepared_key <<= 32;
        prepared_key |= (static_cast<size_t>(key.i) & 0x00000000FFFFFFFFLL);
    }
    return prepared_key;
}

size_t circshift_prepared_key(size_t prepared_key, size_t offset) {
    size_t temp = prepared_key << (sizeof(size_t)-offset)*8;
    return (prepared_key >> offset*8) | temp;
}

void word_mask_exact(char* data,size_t length,const masking_key_type& key) {
    size_t prepared_key = prepare_masking_key(key);
    size_t n = length/sizeof(size_t);
    size_t* word_data = reinterpret_cast<size_t*>(data);
    
    for (size_t i = 0; i < n; i++) {
        word_data[i] ^= prepared_key;
    }
    
    for (size_t i = n*sizeof(size_t); i < length; i++) {
        data[i] ^= key.c[i%4];
    }
}

} // namespace hybi_util
} // namespace processor
} // namespace websocketpp