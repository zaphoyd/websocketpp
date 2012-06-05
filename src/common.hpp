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

#ifndef WEBSOCKET_CONSTANTS_HPP
#define WEBSOCKET_CONSTANTS_HPP

#ifndef __STDC_LIMIT_MACROS
  #define __STDC_LIMIT_MACROS
#endif
//#include <stdint.h>
#include <boost/cstdint.hpp>

// SIZE_MAX appears to be a compiler thing not an OS header thing.
// make sure it is defined.
#ifndef SIZE_MAX
    #define SIZE_MAX ((size_t)(-1))
#endif

#ifdef _MSC_VER
    #ifndef _WEBSOCKETPP_CPP11_FRIEND_
        #define _WEBSOCKETPP_CPP11_FRIEND_
    #endif
#endif

#include <exception>
#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>

// Defaults
namespace websocketpp {
    static const std::string USER_AGENT = "WebSocket++/0.2.0dev";
    
    typedef std::vector<unsigned char> binary_string;
    typedef boost::shared_ptr<binary_string> binary_string_ptr;
    
    typedef std::string utf8_string;
    typedef boost::shared_ptr<utf8_string> utf8_string_ptr;
    
    const uint64_t DEFAULT_MAX_MESSAGE_SIZE = 0xFFFFFF; // ~16MB
    
    const size_t DEFAULT_READ_THRESHOLD = 1; // 512 would be a more sane value for this
    const bool DEFAULT_SILENT_CLOSE = false; // true
    
    const size_t MAX_THREAD_POOL_SIZE = 64;
    
    const uint16_t DEFAULT_PORT = 80;
    const uint16_t DEFAULT_SECURE_PORT = 443;
    
    inline uint16_t default_port(bool secure) {
        return (secure ? DEFAULT_SECURE_PORT : DEFAULT_PORT);
    }   
    
    namespace session {
        namespace state {
            enum value {
                CONNECTING = 0,
                OPEN = 1,
                CLOSING = 2,
                CLOSED = 3
            };
        }
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
    
    namespace fail {
    namespace status {
        enum value {
            GOOD = 0,           // no failure yet!
            SYSTEM = 1,         // system call returned error, check that code
            WEBSOCKET = 2,      // websocket close codes contain error
            UNKNOWN = 3,        // No failure information is avaliable
            TIMEOUT_TLS = 4,    // TLS handshake timed out
            TIMEOUT_WS = 5      // WS handshake timed out
        };
    } // namespace status
    } // namespace fail
    
    namespace frame {
        // Opcodes are 4 bits
        // See spec section 5.2
        namespace opcode {
            enum value {
                CONTINUATION = 0x0,
                TEXT = 0x1,
                BINARY = 0x2,
                RSV3 = 0x3,
                RSV4 = 0x4,
                RSV5 = 0x5,
                RSV6 = 0x6,
                RSV7 = 0x7,
                CLOSE = 0x8,
                PING = 0x9,
                PONG = 0xA,
                CONTROL_RSVB = 0xB,
                CONTROL_RSVC = 0xC,
                CONTROL_RSVD = 0xD,
                CONTROL_RSVE = 0xE,
                CONTROL_RSVF = 0xF
            };
            
            inline bool reserved(value v) {
                return (v >= RSV3 && v <= RSV7) || 
                (v >= CONTROL_RSVB && v <= CONTROL_RSVF);
            }
            
            inline bool invalid(value v) {
                return (v > 0xF || v < 0);
            }
            
            inline bool is_control(value v) {
                return v >= 0x8;
            }
        }
        
        namespace limits {
            static const uint8_t PAYLOAD_SIZE_BASIC = 125;
            static const uint16_t PAYLOAD_SIZE_EXTENDED = 0xFFFF; // 2^16, 65535
            static const uint64_t PAYLOAD_SIZE_JUMBO = 0x7FFFFFFFFFFFFFFFLL;//2^63
        }
    } // namespace frame
    
    // exception class for errors that should be propogated back to the user.
    namespace error {
        enum value {
            GENERIC = 0,
            // send attempted when endpoint write queue was full
            SEND_QUEUE_FULL = 1,
            PAYLOAD_VIOLATION = 2,
            ENDPOINT_UNSECURE = 3,
            ENDPOINT_UNAVAILABLE = 4,
            INVALID_URI = 5,
            NO_OUTGOING_MESSAGES = 6,
            INVALID_STATE = 7
        };
    }
    
    class exception : public std::exception {
    public: 
        exception(const std::string& msg,
                  error::value code = error::GENERIC) 
        : m_msg(msg),m_code(code) {}
        ~exception() throw() {}
        
        virtual const char* what() const throw() {
            return m_msg.c_str();
        }
        
        error::value code() const throw() {
            return m_code;
        }
        
        std::string m_msg;
        error::value m_code;
    };
}

#endif // WEBSOCKET_CONSTANTS_HPP
