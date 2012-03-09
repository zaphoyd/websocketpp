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

#include "chat_client_handler.hpp"

#include <boost/algorithm/string/replace.hpp>

using websocketchat::chat_client_handler;
using websocketpp::client;

void chat_client_handler::on_fail(connection_ptr con) {
    std::cout << "Connection failed" << std::endl;
}

void chat_client_handler::on_open(connection_ptr con) {
    m_con = con;

    std::cout << "Successfully connected" << std::endl;
}

void chat_client_handler::on_close(connection_ptr con) {
    m_con = connection_ptr();

    std::cout << "client was disconnected" << std::endl;
}

void chat_client_handler::on_message(connection_ptr con,message_ptr msg) {
    decode_server_msg(msg->get_payload());
}

// CLIENT API
// client api methods will be called from outside the io_service.run thread
//  they need to be careful to not touch unsyncronized member variables.

void chat_client_handler::send(const std::string &msg) {
    if (!m_con) {
        std::cerr << "Error: no connected session" << std::endl;
        return;
    }
    
    if (msg == "/list") {
        std::cout << "list all participants" << std::endl;
    } else if (msg == "/close") {
        close();
    } else {
        m_con->send(msg);
    }
}

void chat_client_handler::close() {
    if (!m_con) {
        std::cerr << "Error: no connected session" << std::endl;
        return;
    }
    m_con->close(websocketpp::close::status::GOING_AWAY,"");
}

// END CLIENT API


// {"type":"participants","value":[<participant>,...]}
// {"type":"msg","sender":"<sender>","value":"<msg>" }
void chat_client_handler::decode_server_msg(const std::string &msg) {
    // for messages of type participants, erase and rebuild m_participants
    // for messages of type msg, print out message
    
    // NOTE: The chat server was written with the intention of the client having a built in
    // JSON parser. To keep external dependencies low for this demonstration chat client I am
    // parsing the server messages by hand.
    
    std::string::size_type start = 9;
    std::string::size_type end;
    
    if (msg.substr(0,start) != "{\"type\":\"") {
        // ignore
        std::cout << "invalid message" << std::endl;
        return;
    }
    
    

    if (msg.substr(start,15) == "msg\",\"sender\":\"") {
        // parse message
        std::string sender;
        std::string message;
        
        start += 15;

        end = msg.find("\"",start);
        while (end != std::string::npos) {
            if (msg[end-1] == '\\') {
                sender += msg.substr(start,end-start-1) + "\"";
                start = end+1;
                end = msg.find("\"",start);
            } else {
                sender += msg.substr(start,end-start);
                start = end;
                break;
            }
        }
        
        if (msg.substr(start,11) != "\",\"value\":\"") {
            std::cout << "invalid message" << std::endl;
            return;
        }

        start += 11;

        end = msg.find("\"",start);
        while (end != std::string::npos) {
            if (msg[end-1] == '\\') {
                message += msg.substr(start,end-start-1) + "\"";
                start = end+1;
                end = msg.find("\"",start);
            } else {
                message += msg.substr(start,end-start);
                start = end;
                break;
            }
        }

        std::cout << "[" << sender << "] " << message << std::endl;
    } else if (msg.substr(start,23) == "participants\",\"value\":[") {
        // parse participants
        std::cout << "participants message" << std::endl;
    } else {
        // unknown message type
        std::cout << "unknown message" << std::endl;
    }
}
