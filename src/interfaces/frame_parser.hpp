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

#ifndef WEBSOCKET_INTERFACE_FRAME_PARSER_HPP
#define WEBSOCKET_INTERFACE_FRAME_PARSER_HPP

#include <istream>
#include <stdint.h>
#include <exception>

namespace websocketpp {
namespace frame {

namespace error {
	enum value {
		FATAL_SESSION_ERROR = 0,	// force session end
		SOFT_SESSION_ERROR = 1,		// should log and ignore
		PROTOCOL_VIOLATION = 2,		// must end session
		PAYLOAD_VIOLATION = 3,		// should end session
		INTERNAL_SERVER_ERROR = 4,	// cleanly end session
		MESSAGE_TOO_BIG = 5			// ???
	};
}

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
		CONTROL_RSVF = 0xF,
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
	static const uint64_t PAYLOAD_SIZE_JUMBO = 0x7FFFFFFFFFFFFFFF;//2^63
	
	// hardcoded limit
	static const uint64_t INTERNAL_MAX_PAYLOAD_SIZE = 100000000; // 100MB
}
	
class exception : public std::exception {
public:	
	exception(const std::string& msg,
			  frame::error::value code = frame::error::FATAL_SESSION_ERROR) 
	: m_msg(msg),m_code(code) {}
	~exception() throw() {}
	
	virtual const char* what() const throw() {
		return m_msg.c_str();
	}
	
	frame::error::value code() const throw() {
		return m_code;
	}
	
	std::string m_msg;
	frame::error::value m_code;
};
	
class interface {
public:
	// consume
	virtual bool ready() const = 0;
	virtual bool uint64_t get_bytes_needed() const = 0;
	virtual void reset() = 0;
	
	virtual void consume(std::istream& s) = 0;
	
	// retrieve buffers
	
	// read frame options
	
	// Is the last fragment in a message sequence?
	virtual bool get_fin() const = 0;
	virtual opcode::value() const = 0;
	
};


}
}
#endif // WEBSOCKET_INTERFACE_FRAME_PARSER_HPP
