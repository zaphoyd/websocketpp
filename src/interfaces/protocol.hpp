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

#include <boost/shared_ptr.hpp>

namespace websocketpp {
namespace protocol {

class processor {
	// validate client handshake
	// validate server handshake
	
	// Given a list of HTTP headers determine if the values are sufficient
	// to start a websocket session. If so begin constructing a response, if not throw a handshake 
	// exception.
	// validate handshake request
	virtual void validate_handshake(const http::parser::request& headers) const = 0;
	
	virtual void handshake_response(const http::parser::request& headers,http::parser::response& headers) = 0;
	
	// Given a list of HTTP headers determin if the values are a reasonable 
	// response to our handshake request. If so 
	
	// construct
	
	// consume bytes, throw on exception
	virtual void consume(std::istream& s) = 0;
	
	// is there a message ready to be dispatched?
	virtual bool ready() = 0;
	virtual ? get_opcode() = 0;
	
	// consume
	// is_message_complete
	// deliver message (get_payload)
	// some sort of message type? for onping onpong?
};

typedef boost::shared_ptr<processor> processor_ptr;

}
}
#endif // WEBSOCKET_INTERFACE_FRAME_PARSER_HPP
