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

#include "../../src/websocketpp.hpp"

#include <cstring>

using websocketpp::server;

class echo_server_handler : public server::handler {
public:
    void on_message(connection_ptr con, message_ptr msg) {
        con->send(msg->get_payload(),msg->get_opcode());
    }
};

int main(int argc, char* argv[]) {
    unsigned short port = 9002;
        
    if (argc == 2) {
        port = atoi(argv[1]);
        
        if (port == 0) {
            std::cout << "Unable to parse port input " << argv[1] << std::endl;
            return 1;
        }
    }
    
    try {       
        server::handler::ptr h(new echo_server_handler());
        server echo_endpoint(h);
        
        echo_endpoint.alog().unset_level(websocketpp::log::alevel::ALL);
        echo_endpoint.elog().unset_level(websocketpp::log::elevel::ALL);
        
        echo_endpoint.alog().set_level(websocketpp::log::alevel::CONNECT);
        echo_endpoint.alog().set_level(websocketpp::log::alevel::DISCONNECT);
        
        echo_endpoint.elog().set_level(websocketpp::log::elevel::RERROR);
        echo_endpoint.elog().set_level(websocketpp::log::elevel::FATAL);
        
        std::cout << "Starting WebSocket echo server on port " << port << std::endl;
        echo_endpoint.listen(port);
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    
    return 0;
}
