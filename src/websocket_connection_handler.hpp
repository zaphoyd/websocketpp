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
	// Validate is never called for client sessions. To refuse a client session 
	// (ex: if you do not like the set of extensions/subprotocols the server 
	// chose) you can close the connection immediately in the on_open method.
	//
	// handshake_error parameters:
	//  log_message - error message to send to server log
	//  http_error_code - numeric HTTP error code to return to the client
	//  http_error_msg - (optional) string HTTP error code to return to the
	//    client (useful for returning non-standard error codes)
	virtual void validate(session_ptr session) {};

	// on_open is called after the websocket session has been successfully 
	// established and is in the OPEN state. The session is now avaliable to 
	// send messages and will begin reading frames and calling the on_message/
	// on_close/on_error callbacks. A client may reject the connection by 
	// closing the session at this point.
	virtual void on_open(session_ptr session) = 0;

	// on_close is called whenever an open session is closed for any reason. 
	// This can be due to either endpoint requesting a connection close or an 
	// error occuring. Information about why the session was closed can be 
	// extracted from the session itself. 
	// 
	// on_close will be the last time a session calls its handler. If your 
	// application will need information from `session` after this function you
	// should either save the session_ptr somewhere or copy the data out.
	virtual void on_close(session_ptr session) = 0;
	
	// on_message (binary version) will be called when a binary message is 
	// recieved. Message data is passed as a vector of bytes (unsigned char).
	// data will not be avaliable after this callback ends so the handler must 
	// either completely process the message or copy it somewhere else for 
	// processing later.
	virtual void on_message(session_ptr session,
	                        const std::vector<unsigned char> &data) = 0;
	
	// on_message (text version). Identical to on_message except the data 
	// parameter is a string interpreted as UTF-8. WebSocket++ guarantees that
	// this string is valid UTF-8.
	virtual void on_message(session_ptr session,const std::string &msg) = 0;
	
	
	
	// #### optional error cases ####
	
	// on_fail is called whenever a session is terminated or failed before it 
	// was successfully established. This happens if there is an error during 
	// the handshake process or if the server refused the connection.
	// 
	// on_fail will be the last time a session calls its handler. If your 
	// application will need information from `session` after this function you
	// should either save the session_ptr somewhere or copy the data out.
	virtual void on_fail(session_ptr session) {};
	
	// experimental
	virtual void on_ping_timeout(session_ptr session) {}
};



}
#endif // WEBSOCKET_CONNECTION_HANDLER_HPP
