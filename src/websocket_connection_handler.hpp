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

#ifndef WEBSOCKET_CONNECTION_HANDLER_HPP
#define WEBSOCKET_CONNECTION_HANDLER_HPP

#include <boost/shared_ptr.hpp>

#include <string>
#include <map>

namespace websocketpp {
	class connection_handler;
	typedef boost::shared_ptr<connection_handler> connection_handler_ptr;
}

#include "websocket_session.hpp"

namespace websocketpp {

class connection_handler {
public:
	// validate will be called after a websocket handshake has been received and
	// before it is accepted. It provides a handler the ability to refuse a 
	// connection based on application specific logic (ex: restrict domains or
	// negotiate subprotocols). To reject the connection throw a handshake_error
	//
	// handshake_error parameters:
	//  log_message - error message to send to server log
	//  http_error_code - numeric HTTP error code to return to the client
	//  http_error_msg - (optional) string HTTP error code to return to the
	//    client (useful for returning non-standard error codes)
	virtual void validate(session_ptr client) = 0;
	
	// this will be called once the connected websocket is avaliable for 
	// writing messages. client may be a new websocket session or an existing
	// session that was recently passed to this handler.
	virtual void connect(session_ptr client) = 0;
	
	// this will be called when the connected websocket is no longer avaliable
	// for writing messages. This occurs under the following conditions:
	// - Disconnect message recieved from the remote endpoint
	// - Someone (usually this object) calls the disconnect method of session
	// - A disconnect acknowledgement is recieved (in case another object 
	//     calls the disconnect method of session
	// - The connection handler assigned to this client was set to another 
	//     handler
	virtual void disconnect(session_ptr client,uint16_t status,const std::string &reason) = 0;
	
	// this will be called when a text message is recieved. Text will be 
	// encoded as UTF-8.
	virtual void message(session_ptr client,const std::string &msg) = 0;
	
	// this will be called when a binary message is recieved. Argument is a 
	// vector of the raw bytes in the message body.
	virtual void message(session_ptr client,
		const std::vector<unsigned char> &data) = 0;
};



}
#endif // WEBSOCKET_CONNECTION_HANDLER_HPP