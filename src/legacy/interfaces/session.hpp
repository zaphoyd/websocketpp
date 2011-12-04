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

#ifndef WEBSOCKET_INTERFACE_SESSION_HPP
#define WEBSOCKET_INTERFACE_SESSION_HPP

#include <boost/shared_ptr.hpp>

#include <string>
#include <vector>

#include "../websocket_constants.hpp"
#include "../network_utilities.hpp"

namespace websocketpp {	
namespace session {

namespace state {
	enum value {
		CONNECTING = 0,
		OPEN = 1,
		CLOSING = 2,
		CLOSED = 3
	};
}

namespace error {
	enum value {
		FATAL_ERROR = 0,			// force session end
		SOFT_ERROR = 1,				// should log and ignore
		PROTOCOL_VIOLATION = 2,		// must end session
		PAYLOAD_VIOLATION = 3,		// should end session
		INTERNAL_SERVER_ERROR = 4,	// cleanly end session
		MESSAGE_TOO_BIG = 5			// ???
	};
}
	
class exception : public std::exception {
public:	
	exception(const std::string& msg,
			  error::value code = error::FATAL_ERROR) 
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

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                 Server API                              *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
server(uint16_t port, server_handler_ptr handler)
void run();

 
*/
 
 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                             Server Session API                          *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
class server {
public:
	// Valid always
	virtual session::state::value get_state() const = 0;
	virtual unsigned int get_version() const = 0;
	
	virtual std::string get_request_header(const std::string& key) const = 0;
	virtual std::string get_origin() const = 0;
	
	// Information about the requested URI
	virtual bool get_secure() const = 0;
	virtual std::string get_host() const = 0;
	virtual std::string get_resource() const = 0;
	virtual uint16_t get_port() const = 0;
	
	// Information about the connected endpoint
	/* Tentative API member function */ virtual boost::asio::ip::tcp::endpoint get_endpoint() const = 0;
	
	// Valid for CONNECTING state
	virtual void add_response_header(const std::string& key, const std::string& value) = 0;
	virtual void replace_response_header(const std::string& key, const std::string& value) = 0;
	virtual const std::vector<std::string>& get_subprotocols() const = 0;
	virtual const std::vector<std::string>& get_extensions() const = 0;
	virtual void select_subprotocol(const std::string& value) = 0;
	virtual void select_extension(const std::string& value) = 0;
	
	// Valid for OPEN state
	virtual void send(const utf8_string& payload) = 0;
	virtual void send(const binary_string& data) = 0;
	virtual void close(close::status::value code, const utf8_string& reason) = 0;
	virtual void ping(const binary_string& payload) = 0;
	virtual void pong(const binary_string& payload) = 0;
	
	virtual uint64_t buffered_amount() const = 0;
	
	// Valid for CLOSED state
	virtual close::status::value get_local_close_code() const = 0;
	virtual utf8_string get_local_close_reason() const = 0;
	virtual close::status::value get_remote_close_code() const = 0;
	virtual utf8_string get_remote_close_reason() const = 0;
	virtual bool get_failed_by_me() const = 0;
	virtual bool get_dropped_by_me() const = 0;
	virtual bool get_closed_by_me() const = 0;
};
typedef boost::shared_ptr<server> server_ptr;
typedef server_ptr server_session_ptr;
	
 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                             Server Handler API                          *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
class server_handler {
public:
	virtual ~server_handler() {}
	
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
	virtual void validate(server_ptr session) = 0;
	
	// on_open is called after the websocket session has been successfully 
	// established and is in the OPEN state. The session is now avaliable to 
	// send messages and will begin reading frames and calling the on_message/
	// on_close/on_error callbacks. A client may reject the connection by 
	// closing the session at this point.
	virtual void on_open(server_ptr session) = 0;
	
	// on_close is called whenever an open session is closed for any reason. 
	// This can be due to either endpoint requesting a connection close or an 
	// error occuring. Information about why the session was closed can be 
	// extracted from the session itself. 
	// 
	// on_close will be the last time a session calls its handler. If your 
	// application will need information from `session` after this function you
	// should either save the session_ptr somewhere or copy the data out.
	virtual void on_close(server_ptr session) = 0;
	
	// on_message (binary version) will be called when a binary message is 
	// recieved. Message data is passed as a vector of bytes (unsigned char).
	// data will not be avaliable after this callback ends so the handler must 
	// either completely process the message or copy it somewhere else for 
	// processing later.
	virtual void on_message(server_ptr session, binary_string_ptr data) = 0;
	
	// on_message (text version). Identical to on_message except the data 
	// parameter is a string interpreted as UTF-8. WebSocket++ guarantees that
	// this string is valid UTF-8.
	virtual void on_message(server_ptr session, utf8_string_ptr msg) = 0;
	
	// on_fail is called whenever a session is terminated or failed before it 
	// was successfully established. This happens if there is an error during 
	// the handshake process or if the server refused the connection.
	// 
	// on_fail will be the last time a session calls its handler. If your 
	// application will need information from `session` after this function you
	// should either save the session_ptr somewhere or copy the data out.
	virtual void on_fail(server_ptr session) {};
	
	// on_ping is called whenever a ping is recieved with the binary 
	// application data from the ping. If on_ping returns true the
	// implimentation will send a response pong.
	virtual bool on_ping(server_ptr session, binary_string_ptr data) {
		return true;
	}
	
	// on_pong is called whenever a pong is recieved with the binary 
	// application data from the pong.
	virtual void on_pong(server_ptr session, binary_string_ptr data) {}
	
	// TODO: on_ping_timeout
};

typedef boost::shared_ptr<server_handler> server_handler_ptr;

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                             Client Session API                          *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
	
class client {
public:
	client(const std::string& uri) {};
	
	// Valid always
	virtual session::state::value get_state() const = 0;
	virtual int get_version() const = 0;
	
	virtual std::string get_origin() const = 0;
	virtual bool get_secure() const = 0;
	virtual std::string get_host() const = 0;
	virtual std::string get_resource() const = 0;
	virtual uint16_t get_port() const = 0;
	
	// Valid for CONNECTING state
	virtual void set_origin(const std::string& origin) = 0;
	virtual void add_request_header(const std::string& key, const std::string& value) = 0;
	virtual void replace_request_header(const std::string& key, const std::string& value) = 0;
	virtual void request_subprotocol(const std::string& value) = 0;
	virtual void request_extension(const std::string& value) = 0;
	
	// Valid for OPEN state
	virtual std::string get_response_header(const std::string& key) const = 0;
	virtual std::string get_subprotocol() const;
	virtual const std::vector<std::string>& get_extensions() const = 0;
	
	virtual void send(const utf8_string& msg) = 0;
	virtual void send(const binary_string& data) = 0;
	virtual void close(close::status::value code, const binary_string& reason) = 0;
	virtual void ping(const binary_string& payload) = 0;
	virtual void pong(const binary_string& payload) = 0;
	
	// Valid for CLOSED state
	virtual close::status::value get_local_close_code() const = 0;
	virtual utf8_string get_local_close_reason() const = 0;
	virtual close::status::value get_remote_close_code() const = 0;
	virtual utf8_string get_remote_close_reason() const = 0;
	virtual bool failed_by_me() const = 0;
	virtual bool dropped_by_me() const = 0;
	virtual bool closed_by_me() const = 0;
};

typedef boost::shared_ptr<client> client_ptr;

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                             Client Handler API                          *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
	
class client_handler {
public:
	// on_open is called after the websocket session has been successfully 
	// established and is in the OPEN state. The session is now avaliable to 
	// send messages and will begin reading frames and calling the on_message/
	// on_close/on_error callbacks. A client may reject the connection by 
	// closing the session at this point.
	virtual void on_open(client_ptr session) = 0;
	
	// on_close is called whenever an open session is closed for any reason. 
	// This can be due to either endpoint requesting a connection close or an 
	// error occuring. Information about why the session was closed can be 
	// extracted from the session itself. 
	// 
	// on_close will be the last time a session calls its handler. If your 
	// application will need information from `session` after this function you
	// should either save the session_ptr somewhere or copy the data out.
	virtual void on_close(client_ptr session) = 0;
	
	// on_message (binary version) will be called when a binary message is 
	// recieved. Message data is passed as a vector of bytes (unsigned char).
	// data will not be avaliable after this callback ends so the handler must 
	// either completely process the message or copy it somewhere else for 
	// processing later.
	virtual void on_message(client_ptr session, binary_string_ptr data) = 0;
	
	// on_message (text version). Identical to on_message except the data 
	// parameter is a string interpreted as UTF-8. WebSocket++ guarantees that
	// this string is valid UTF-8.
	virtual void on_message(client_ptr session, utf8_string_ptr msg) = 0;
	
	// on_fail is called whenever a session is terminated or failed before it 
	// was successfully established. This happens if there is an error during 
	// the handshake process or if the server refused the connection.
	// 
	// on_fail will be the last time a session calls its handler. If your 
	// application will need information from `session` after this function you
	// should either save the session_ptr somewhere or copy the data out.
	virtual void on_fail(client_ptr session) {};
	
	// on_ping is called whenever a ping is recieved with the binary 
	// application data from the ping. If on_ping returns true the
	// implimentation will send a response pong.
	virtual bool on_ping(server_ptr session, binary_string_ptr data) {
		return true;
	}
	
	// on_pong is called whenever a pong is recieved with the binary 
	// application data from the pong.
	virtual void on_pong(server_ptr session, binary_string_ptr data) {}
	
	// TODO: on_ping_timeout
};

typedef boost::shared_ptr<client_handler> client_handler_ptr;
	
}
}
#endif // WEBSOCKET_INTERFACE_SESSION_HPP
