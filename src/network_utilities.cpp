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

#include "network_utilities.hpp"



uint64_t zsutil::htonll(uint64_t src) { 
    static int typ = TYP_INIT; 
    unsigned char c; 
    union { 
        uint64_t ull; 
        unsigned char c[8]; 
    } x; 
    if (typ == TYP_INIT) { 
        x.ull = 0x01; 
        typ = (x.c[7] == 0x01ULL) ? TYP_BIGE : TYP_SMLE; 
    } 
    if (typ == TYP_BIGE) 
        return src; 
    x.ull = src; 
    c = x.c[0]; x.c[0] = x.c[7]; x.c[7] = c; 
    c = x.c[1]; x.c[1] = x.c[6]; x.c[6] = c; 
    c = x.c[2]; x.c[2] = x.c[5]; x.c[5] = c; 
    c = x.c[3]; x.c[3] = x.c[4]; x.c[4] = c; 
    return x.ull; 
}

uint64_t zsutil::ntohll(uint64_t src) { 
    return htonll(src);
}

std::string zsutil::lookup_ws_close_status_string(uint16_t code) {
    switch (code) {
        case 1000: 
            return "Normal closure";
        case 1001: 
            return "Going away";
        case 1002: 
            return "Protocol error";
        case 1003: 
            return "Unacceptable data";
        case 1004: 
            return "Reserved";
        case 1005: 
            return "No status received";
        case 1006: 
            return "Abnormal closure";
        case 1007: 
            return "Invalid message data";
        case 1008: 
            return "Policy Violation";
        case 1009: 
            return "Message too large";
        case 1010: 
            return "Missing required extensions";
        case 1011: 
            return "Internal server error";
        default:
            return "Unknown";
    }
}

std::string zsutil::to_hex(const std::string& input) {
    std::string output;
    std::string hex = "0123456789ABCDEF";
    
    for (size_t i = 0; i < input.size(); i++) {
        output += hex[(input[i] & 0xF0) >> 4];
        output += hex[input[i] & 0x0F];
        output += " ";
    }
    
    return output;
}

std::string zsutil::to_hex(const char* input,size_t length) {
    std::string output;
    std::string hex = "0123456789ABCDEF";
    
    for (size_t i = 0; i < length; i++) {
        output += hex[(input[i] & 0xF0) >> 4];
        output += hex[input[i] & 0x0F];
        output += " ";
    }
    
    return output;
}
