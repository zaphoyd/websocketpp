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

#include "../http/parser.hpp"

#include <boost/shared_ptr.hpp>

#include <iostream>

namespace websocketpp {
namespace protocol {

class processor {
public:
	// validate client handshake
	// validate server handshake
	
	// Given a list of HTTP headers determine if the values are sufficient
	// to start a websocket session. If so begin constructing a response, if not throw a handshake 
	// exception.
	// validate handshake request
	virtual void validate_handshake(const http::parser::request& headers) const = 0;
	
	virtual void handshake_response(const http::parser::request& request,http::parser::response& response) = 0;
	
	// Extracts client origin from a handshake request
	virtual std::string get_origin(const http::parser::request& request) const = 0;
	// Extracts client uri from a handshake request
	virtual ws_uri get_uri(const http::parser::request& request) const = 0;
	
	// consume bytes, throw on exception
	virtual void consume(std::istream& s) = 0;
	
	// is there a message ready to be dispatched?
	virtual bool ready() const = 0;
	virtual void reset() = 0;
	
	virtual uint64_t get_bytes_needed() const = 0;
	
	// Get information about the message that is ready
	virtual frame::opcode::value get_opcode() const = 0;
	
	virtual utf8_string_ptr get_utf8_payload() const = 0;
	virtual binary_string_ptr get_binary_payload() const = 0;
	virtual close::status::value get_close_code() const = 0;
	virtual utf8_string get_close_reason() const = 0;
	
	// TODO: prepare a frame
	virtual binary_string_ptr prepare_frame(frame::opcode::value opcode,
											bool mask,
											const utf8_string& payload)  = 0;
	virtual binary_string_ptr prepare_frame(frame::opcode::value opcode,
											bool mask,
											const binary_string& payload)  = 0;
	
	virtual binary_string_ptr prepare_close_frame(close::status::value code,
												  bool mask,
												  const std::string& reason) = 0;
	
	
	
};

typedef boost::shared_ptr<processor> processor_ptr;

}
}
#endif // WEBSOCKET_INTERFACE_FRAME_PARSER_HPP
