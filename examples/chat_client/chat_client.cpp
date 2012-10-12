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

#include "../../src/roles/client.hpp"
#include "../../src/websocketpp.hpp"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include <iostream>

using boost::asio::ip::tcp;
using websocketpp::client;

using namespace websocketchat;

int main(int argc, char* argv[]) {
    std::string uri;
    
    if (argc != 2) {
        std::cout << "Usage: `chat_client ws_uri`" << std::endl;
    } else {
        uri = argv[1];
    }
        
    try {
        chat_client_handler_ptr handler(new chat_client_handler());
        client endpoint(handler);
        client::connection_ptr con;
        
        endpoint.alog().unset_level(websocketpp::log::alevel::ALL);
        endpoint.elog().unset_level(websocketpp::log::elevel::ALL);
        
        endpoint.elog().set_level(websocketpp::log::elevel::RERROR);
        endpoint.elog().set_level(websocketpp::log::elevel::FATAL);
        
        con = endpoint.get_connection(uri);
        
        con->add_request_header("User-Agent","WebSocket++/0.2.0 WebSocket++Chat/0.2.0");
        con->add_subprotocol("com.zaphoyd.websocketpp.chat");
        
        con->set_origin("http://zaphoyd.com");

        endpoint.connect(con);
        
        boost::thread t(boost::bind(&client::run, &endpoint, false));
        
        char line[512];
        while (std::cin.getline(line, 512)) {
            handler->send(line);
        }
        
        t.join();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    
    return 0;
}
