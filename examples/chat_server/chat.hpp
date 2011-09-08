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

#ifndef CHAT_HPP
#define CHAT_HPP

// com.zaphoyd.websocketpp.chat protocol
// 
// client messages:
// msg [UTF8 text]
// 
// server messages:
// {"type":"msg","sender":"<sender>","value":"<msg>" }
// {"type":"participants","value":[<participant>,...]}

#include <boost/shared_ptr.hpp>

#include <websocketpp/websocket_connection_handler.hpp>

#include <set>
#include <map>
#include <string>
#include <vector>

namespace websocketchat {

class chat_handler : public websocketpp::connection_handler {
public:
	chat_handler() {}
	virtual ~chat_handler() {}
	
	bool validate(websocketpp::session_ptr client); 
	
	// add new connection to the lobby
	void connect(session_ptr client) {
		std::cout << "client " << client << " joined the lobby." << std::endl;
		m_connections.insert(client);

		// send user list to all clients
		// send signon message from server to all other clients.

	}
		
	// someone disconnected from the lobby, remove them
	void disconnect(websocketpp::session_ptr client,const std::string &reason) {
		std::set<session_ptr>::iterator it = m_connections.find(client);
		
		if (it == m_connections.end()) {
			// this client has already disconnected, we can ignore this.
			// this happens during certain types of disconnect where there is a
			// deliberate "soft" disconnection preceeding the "hard" socket read
			// fail or disconnect ack message.
			return;
		}
		
		std::cout << "client " << client << " left the lobby." << std::endl;
		
		m_connections.erase(it);

		// send signoff message from server to all clients
		// send user list to all remaining clients
	}
	
	void message(websocketpp::session_ptr client,const std::string &msg) {
		std::cout << "message from client " << client << ": " << msg << std::endl;
		
		// create JSON message to send based on msg

		for (std::set<session_ptr>::iterator it = m_connections.begin();
												it != m_connections.end(); it++) {
			(*it)->send(msg);
		}
	}
	
	// lobby will ignore binary messages
	void message(websocketpp::session_ptr client,
		const std::vector<unsigned char> &data) {}
private:
	std::string serialize_state();

	// list of outstanding connections
	std::set<session_ptr> m_connections;
};

typedef boost::shared_ptr<chat_handler> chat_handler_ptr;

}
#endif // CHAT_HPP
