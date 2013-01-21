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

#ifndef WEBSOCKETPP_LOGGER_HPP
#define WEBSOCKETPP_LOGGER_HPP

/* Need a way to print a message to the log
 * 
 * - timestamps
 * - channels
 * - thread safe
 * - output to stdout or file
 * - selective output channels, both compile time and runtime
 * - named channels
 * - ability to test whether a log message will be printed at compile time
 * 
 */

#include <ctime>

namespace websocketpp {
namespace logger {

struct error_names {
    static const uint32_t none = 0x0;
    static const uint32_t devel = 0x1;
    static const uint32_t library = 0x2;
    static const uint32_t info = 0x4;
    static const uint32_t warn = 0x8;
    static const uint32_t rerror = 0x10;
    static const uint32_t fatal = 0x20;
    static const uint32_t all = 0xffffffff;
    
    static const char* channel_name(uint32_t channel) {
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

struct access_names {
    static const uint32_t none = 0x0;
    static const uint32_t connect = 0x1;
    static const uint32_t disconnect = 0x2;
    static const uint32_t control = 0x4;
    static const uint32_t frame_header = 0x8;
    static const uint32_t frame_payload = 0x10;
    static const uint32_t message_header = 0x20;
    static const uint32_t message_payload = 0x40;
    static const uint32_t endpoint = 0x80;
    static const uint32_t debug_handshake = 0x100;
    static const uint32_t debug_close = 0x200;
    static const uint32_t devel = 0x400;
    static const uint32_t all = 0xffffffff;
    
    static const char* channel_name(uint32_t channel) {
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
	        default:
	            return "unknown";
	    }
	}
};

template <typename concurrency, typename names>
class basic {
public:
    basic<concurrency,names>() 
      : m_static_channels(0xffffffff)
      , m_dynamic_channels(0) {}
    
    basic<concurrency,names>(uint32_t c)
      : m_static_channels(c)
      , m_dynamic_channels(0) {}
    
    void set_channels(uint32_t channels) {
        scoped_lock_type lock(m_lock);
        m_dynamic_channels |= (channels & m_static_channels);
    }
    
    void clear_channels(uint32_t channels) {
        scoped_lock_type lock(m_lock);
        m_dynamic_channels &= channels;
    }
    
    void write(uint32_t channel, const std::string& msg) {
        scoped_lock_type lock(m_lock);
        if (!this->dynamic_test(channel)) { return; }
        std::cout << "[" << get_timestamp() << "] " 
                  << "[" << names::channel_name(channel) << "] " 
                  << msg << std::endl;
    }
    
    void write(uint32_t channel, const char* msg) {
        scoped_lock_type lock(m_lock);
        if (!this->dynamic_test(channel)) { return; }
        std::cout << "[" << get_timestamp() << "] "
                  << "[" << names::channel_name(channel) << "] " 
                  << msg << std::endl;
    }
    
    bool static_test(uint32_t channel) const {
        return channel & m_static_channels;
    }
    
private:
    typedef typename concurrency::scoped_lock_type scoped_lock_type;
	typedef typename concurrency::mutex_type mutex_type;
	
	bool dynamic_test(uint32_t channel) {
	    return channel & m_dynamic_channels;
	}
	
    const char* get_timestamp() {
        std::time_t t = std::time(NULL);
        std::strftime(buffer,30,"%Y-%m-%d %H:%M:%S%z",std::localtime(&t));
        return buffer;
    }

	mutex_type m_lock;
	
    char buffer[30];
    const uint32_t m_static_channels;
    uint32_t m_dynamic_channels;
};

} // logger
} // websocketpp

#endif //WEBSOCKETPP_LOGGER_HPP
