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

#ifndef WEBSOCKETPP_MD5_WRAPPER_HPP
#define WEBSOCKETPP_MD5_WRAPPER_HPP

#include "md5.h"
#include <string>

namespace websocketpp {

// could be compiled separately
inline std::string md5_hash_string(const std::string& s) {
	char digest[16];
	
	md5_state_t state;
	
	md5_init(&state);
	md5_append(&state, (const md5_byte_t *)s.c_str(), s.size());
	md5_finish(&state, (md5_byte_t *)digest);
	    
    std::string ret;
    ret.resize(16);
    std::copy(digest,digest+16,ret.begin());
    
	return ret;
}

const char hexval[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

inline std::string md5_hash_hex(const std::string& input) {
    std::string hash = md5_hash_string(input);
    std::string hex;
        
    for (size_t i = 0; i < hash.size(); i++) {
        hex.push_back(hexval[((hash[i] >> 4) & 0xF)]);
        hex.push_back(hexval[(hash[i]) & 0x0F]);
    }
    
    return hex;
}

} // websocketpp

#endif // WEBSOCKETPP_MD5_WRAPPER_HPP
