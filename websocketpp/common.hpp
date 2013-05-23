/*
 * Copyright (c) 2013, Peter Thorson. All rights reserved.
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

#ifndef WEBSOCKETPP_COMMON_HPP
#define WEBSOCKETPP_COMMON_HPP

#include <string>

namespace websocketpp {

// Size of the transport read buffer. Increasing may improve performance for
// large reads. Higher values will increase memory usage linearly with 
// connection count.

const size_t DEFAULT_READ_THRESHOLD = 1; // 512 would be a more sane value

// Maximum size in bytes before rejecting an HTTP header as too big.
// Is defined in websocketpp/http/constants.hpp

// Default user agent.





}

namespace close {
namespace status {
    enum value {
        INVALID_END = 999,
        NORMAL = 1000,
        GOING_AWAY = 1001,
        PROTOCOL_ERROR = 1002,
        UNSUPPORTED_DATA = 1003,
        RSV_ADHOC_1 = 1004,
        NO_STATUS = 1005,
        ABNORMAL_CLOSE = 1006,
        INVALID_PAYLOAD = 1007,
        POLICY_VIOLATION = 1008,
        MESSAGE_TOO_BIG = 1009,
        EXTENSION_REQUIRE = 1010,
        INTERNAL_ENDPOINT_ERROR = 1011,
        RSV_ADHOC_2 = 1012,
        RSV_ADHOC_3 = 1013,
        RSV_ADHOC_4 = 1014,
        TLS_HANDSHAKE = 1015,
        RSV_START = 1016,
        RSV_END = 2999,
        INVALID_START = 5000
    };
    
    inline bool reserved(value s) {
        return ((s >= RSV_START && s <= RSV_END) || s == RSV_ADHOC_1 
                || s == RSV_ADHOC_2 || s == RSV_ADHOC_3 || s == RSV_ADHOC_4);
    }
    
    // Codes invalid on the wire
    inline bool invalid(value s) {
        return ((s <= INVALID_END || s >= INVALID_START) || 
                s == NO_STATUS || 
                s == ABNORMAL_CLOSE || 
                s == TLS_HANDSHAKE);
    }
    
    // TODO functions for application ranges?
} // namespace status
} // namespace close



#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/functional.hpp>
#include <websocketpp/common/regex.hpp>
#include <websocketpp/common/system_error.hpp>

#endif // WEBSOCKETPP_COMMON_HPP
