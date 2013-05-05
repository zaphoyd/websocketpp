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

#ifndef WEBSOCKETPP_LOGGER_LEVELS_HPP
#define WEBSOCKETPP_LOGGER_LEVELS_HPP

#include <websocketpp/common/stdint.hpp>

namespace websocketpp {
namespace log {

typedef uint32_t level;

struct elevel {
    static const level none = 0x0;
    static const level devel = 0x1;
    static const level library = 0x2;
    static const level info = 0x4;
    static const level warn = 0x8;
    static const level rerror = 0x10;
    static const level fatal = 0x20;
    static const level all = 0xffffffff;
    
    static const char* channel_name(level channel) {
        switch(channel) {
            case devel:
                return "devel";
            case library:
                return "library";
            case info:
                return "info";
            case warn:
                return "warning";
            case rerror:
                return "error";
            case fatal:
                return "fatal";
            default:
                return "unknown";
        }
    }
};

struct alevel {
    static const level none = 0x0;
    static const level connect = 0x1;
    static const level disconnect = 0x2;
    static const level control = 0x4;
    static const level frame_header = 0x8;
    static const level frame_payload = 0x10;
    static const level message_header = 0x20;
    static const level message_payload = 0x40;
    static const level endpoint = 0x80;
    static const level debug_handshake = 0x100;
    static const level debug_close = 0x200;
    static const level devel = 0x400;
    static const level app = 0x800;
    static const level all = 0xffffffff;
    
    static const char* channel_name(level channel) {
        switch(channel) {
            case connect:
                return "connect";
            case disconnect:
                return "disconnect";
            case control:
                return "control";
            case frame_header:
                return "frame_header";
            case frame_payload:
                return "frame_payload";
            case message_header:
                return "message_header";
            case message_payload:
                return "message_payload";
            case endpoint:
                return "endpoint";
            case debug_handshake:
                return "debug_handshake";
            case debug_close:
                return "debug_close";
            case devel:
                return "devel";
            case app:
                return "application";
            default:
                return "unknown";
        }
    }
};

} // logger
} // websocketpp

#endif //WEBSOCKETPP_LOGGER_LEVELS_HPP
