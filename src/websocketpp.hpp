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
 * This Makefile was derived from a similar one included in the libjson project
 * It's authors were Jonathan Wallace and Bernhard Fluehmann.
 */

#ifndef WEBSOCKETPP_HPP
#define WEBSOCKETPP_HPP

#define __STDC_LIMIT_MACROS 
#include <stdint.h>

// Defaults
namespace websocketpp {
	const uint64_t DEFAULT_MAX_MESSAGE_SIZE = 0xFFFFFF; // ~16MB
	
	// System logging levels
	static const uint16_t LOG_ALL = 0;
	static const uint16_t LOG_DEBUG = 1;
	static const uint16_t LOG_INFO = 2;
	static const uint16_t LOG_WARN = 3;
	static const uint16_t LOG_ERROR = 4;
	static const uint16_t LOG_FATAL = 5;
	static const uint16_t LOG_OFF = 6;
	
	// Access logging controls
	// Individual bits
	static const uint16_t ALOG_CONNECT = 0x1;
	static const uint16_t ALOG_DISCONNECT = 0x2;
	static const uint16_t ALOG_MISC_CONTROL = 0x4;
	static const uint16_t ALOG_FRAME = 0x8;
	static const uint16_t ALOG_MESSAGE = 0x10;
	static const uint16_t ALOG_INFO = 0x20;
	static const uint16_t ALOG_HANDSHAKE = 0x40;
	// Useful groups
	static const uint16_t ALOG_OFF = 0x0;
	static const uint16_t ALOG_CONTROL = ALOG_CONNECT 
									   & ALOG_DISCONNECT 
									   & ALOG_MISC_CONTROL;
	static const uint16_t ALOG_ALL = 0xFFFF;
	
	
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
			RSV_START = 1011,
			RSV_END = 2999,
			INVALID_START = 5000
		};
		
		inline bool reserved(uint16_t s) {
			return ((s >= RSV_START && s <= RSV_END) || 
					s == RSV_ADHOC_1);
		}
		
		inline bool invalid(uint16_t s) {
			return ((s <= INVALID_END || s >= INVALID_START) || 
					s == NO_STATUS || 
					s == ABNORMAL_CLOSE);
		}
	}
	}

}

#include "websocket_session.hpp"
#include "websocket_server_session.hpp"
#include "websocket_client_session.hpp"
#include "websocket_server.hpp"
#include "websocket_client.hpp"

#endif // WEBSOCKETPP_HPP
