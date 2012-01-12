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
// alias [UTF8 text, 16 characters max]
// msg [UTF8 text]
// 
// server messages:
// {"type":"msg","sender":"<sender>","value":"<msg>" }
// {"type":"participants","value":[<participant>,...]}

#include "../../src/websocketpp.hpp"
#include "../../src/interfaces/session.hpp"
#include <boost/shared_ptr.hpp>

#include <map>
#include <string>
#include <vector>

using websocketpp::session::server_session_ptr;
using websocketpp::session::server_handler;

namespace websocketchat {

class chat_server_handler : public server_handler {
public:
    void validate(server_session_ptr session); 
    
    // add new connection to the lobby
    void on_open(server_session_ptr session);
        
    // someone disconnected from the lobby, remove them
    void on_close(server_session_ptr session);
    
    void on_message(server_session_ptr session,websocketpp::utf8_string_ptr msg);
    
    // lobby will ignore binary messages
    void on_message(server_session_ptr session,websocketpp::binary_string_ptr data) {}
private:
    std::string serialize_state();
    std::string encode_message(std::string sender,std::string msg,bool escape = true);
    std::string get_con_id(server_session_ptr s);
    
    void send_to_all(std::string data);
    
    // list of outstanding connections
    std::map<server_session_ptr,std::string> m_connections;
};

typedef boost::shared_ptr<chat_server_handler> chat_server_handler_ptr;

}
#endif // CHAT_HPP
