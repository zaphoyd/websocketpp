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

#include "../../src/roles/client.hpp"
#include "../../src/websocketpp.hpp"

#include <iostream>

using websocketpp::client;

class echo_client_handler : public client::handler {
public:
    void on_message(connection_ptr con, message_ptr msg) {        
        if (con->get_resource() == "/getCaseCount") {
            std::cout << "Detected " << msg->get_payload() << " test cases." << std::endl;
            m_case_count = atoi(msg->get_payload().c_str());
        } else {
            con->send(msg->get_payload(),msg->get_opcode());
        }
    }
    
    void on_fail(connection_ptr con) {
        std::cout << "connection failed" << std::endl;
    }
    
    int m_case_count;
};


int main(int argc, char* argv[]) {
    std::string uri = "ws://localhost:9001/";
    
    if (argc == 2) {
        uri = argv[1];
        
    } else if (argc > 2) {
        std::cout << "Usage: `echo_client test_url`" << std::endl;
    }
    
    try {
        client::handler::ptr handler(new echo_client_handler());
        client::connection_ptr con;
        client endpoint(handler);
        
        endpoint.alog().unset_level(websocketpp::log::alevel::ALL);
        endpoint.elog().unset_level(websocketpp::log::elevel::ALL);
        
        con = endpoint.connect(uri+"getCaseCount");
                
        endpoint.run();
        
        std::cout << "case count: " << boost::dynamic_pointer_cast<echo_client_handler>(handler)->m_case_count << std::endl;
        
        for (int i = 1; i <= boost::dynamic_pointer_cast<echo_client_handler>(handler)->m_case_count; i++) {
            endpoint.reset();
            
            std::stringstream url;
            
            url << uri << "runCase?case=" << i << "&agent=WebSocket++/0.2.0-dev";
                        
            con = endpoint.connect(url.str());
            
            endpoint.run();
        }
        
        std::cout << "done" << std::endl;
        
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    
    return 0;
}
