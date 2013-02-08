
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

#ifndef WEBSOCKETPP_CLOSE_HPP
#define WEBSOCKETPP_CLOSE_HPP

#include <websocketpp/error.hpp> 
#include <websocketpp/common/network.hpp>
#include <websocketpp/common/stdint.hpp>
#include <websocketpp/utf8_validator.hpp> 

#include <string>

namespace websocketpp {
namespace close {
namespace status {
    typedef uint16_t value;
    
    // A blank value for internal use
    static const value blank = 0;

    // Codes from RFC6455
    static const value normal = 1000;
    static const value going_away = 1001;
    static const value protocol_error = 1002;
    static const value unsupported_data = 1003;
    static const value no_status = 1005;
    static const value abnormal_close = 1006;
    static const value invalid_payload = 1007;
    static const value policy_violation = 1008;
    static const value message_too_big = 1009;
    static const value extension_required = 1010;
    static const value internal_endpoint_error = 1011;     
    static const value tls_handshake = 1015; // is this official?     
    
    /// Ranges of values that are reserved for future protocol use
    static const value rsv_start = 1016;
    static const value rsv_end = 2999;
    
    /// Test whether a close code is in a reserved range
    inline bool reserved(value s) {
        return ((s >= rsv_start && s <= rsv_end) || 
                s == 1004 || s == 1012 || s == 1013 || s == 1014);
    }
    
    // Ranges of values that are always invalid on the wire
    static const value invalid_low = 999;
    static const value invalid_high = 5000;
        
    // Test whether a close code is invalid on the wire
    inline bool invalid(value s) {
        return (s <= invalid_low ||s >= invalid_high || 
                s == no_status || s == abnormal_close || s == tls_handshake);
    }
    
    /// There is a class of errors for which once they are discovered normal
    /// WebSocket functionality can no longer occur. This function determines
    /// if a given code is one of these values. This information is used to
    /// determine if the system has the capability of waiting for a close
    /// acknowledgement or if it should drop the TCP connection immediately 
    /// after sending its close frame.
    inline bool terminal(value s) {
        return (s == protocol_error || s == invalid_payload || 
                policy_violation || message_too_big || 
                 internal_endpoint_error);
    }
} // namespace status

union code_converter {
    uint16_t i;
    char c[2];
};

/// Extract a close code value from a close payload
/**
 * If there is no close value (ie string is empty) status::no_status is 
 * returned. If a code couldn't be extracted (usually do to a short or
 * otherwise mangled payload) status::protocol_error is returned and the ec
 * value is flagged as an error. Note that this case is different than the case
 * where protocol error is received over the wire.
 *
 * If the value is in an invalid or reserved range ec is set accordingly
 *
 * @param payload Close frame payload value recieved over the wire.
 * 
 * @param ec A status code, zero on success, non-zero on failure or partial 
 * success. See notes above.
 *
 * @return The extracted value
 */
inline status::value extract_code(const std::string& payload, lib::error_code & ec) {
    ec = lib::error_code();

    if (payload.size() == 0) {
        return status::no_status;
    } else if (payload.size() == 1) {
        ec = make_error_code(error::bad_close_code);
        return status::protocol_error;
    }

    code_converter val;

    val.c[0] = payload[0];
    val.c[1] = payload[1];
    
    status::value code(ntohs(val.i));    
    
    if (status::invalid(code)) {
        ec = make_error_code(error::invalid_close_code);
    }
    
    if (status::reserved(code)) {
        ec = make_error_code(error::reserved_close_code);
    }
   
    return code;
}

/// Extract the reason string from a close payload
/**
 * Extract the reason string from a close payload message. The string should be
 * A valid utf8 message. An invalid_utf8 error will be set if the function
 * extracts a reason that is not valid utf8.
 *
 * @param payload The payload string to extract a reason from.
 *
 * @param ec A place to store error values, zero on success, non-zero otherwise
 *
 * @return the reason string.
 */
inline std::string extract_reason(const std::string& payload, lib::error_code & ec) {
    std::string reason = "";    
    ec = lib::error_code();

    if (payload.size() > 2) {
        reason.append(payload.begin()+2,payload.end());
    }
    
    if (!websocketpp::utf8_validator::validate(reason)) {
        ec = make_error_code(error::invalid_utf8);
    }
    
    return reason;
}

} // namespace close
} // namespace websocketpp

#endif // WEBSOCKETPP_CLOSE_HPP
