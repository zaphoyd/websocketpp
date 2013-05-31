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

class sip_client_handler : public client::handler {
public:

    bool received;

    void on_open(connection_ptr con) {
        // now it is safe to use the connection
        std::cout << "connection ready" << std::endl;

        received = false;

        std::string SIP_msg="OPTIONS sip:carol@chicago.com SIP/2.0\r\nVia: SIP/2.0/WS df7jal23ls0d.invalid;rport;branch=z9hG4bKhjhs8ass877\r\nMax-Forwards: 70\r\nTo: <sip:carol@chicago.com>\r\nFrom: Alice <sip:alice@atlanta.com>;tag=1928301774\r\nCall-ID: a84b4c76e66710\r\nCSeq: 63104 OPTIONS\r\nContact: <sip:alice@pc33.atlanta.com>\r\nAccept: application/sdp\r\nContent-Length: 0\r\n\r\n";
        con->send(SIP_msg.c_str());
    }

    void on_close(connection_ptr con) {
        // no longer safe to use the connection
        std::cout << "connection closed" << std::endl;
    }

    void on_message(connection_ptr con, message_ptr msg) {        
        std::cout << msg->get_payload()  << std::endl; 
        received=true;
    }
    
    void on_fail(connection_ptr con) {
        std::cout << "connection failed" << std::endl;
    }
};


int main(int argc, char* argv[]) {
    std::string uri = "ws://localhost:9001/";
    
    if (argc == 2) {
        uri = argv[1];
        
    } else if (argc > 2) {
        std::cout << "Usage: `sip_client test_url`" << std::endl;
    }
    
    try {
        client::handler::ptr handler(new sip_client_handler());
        client::connection_ptr con;
        client endpoint(handler);
        
        endpoint.alog().unset_level(websocketpp::log::alevel::ALL);
        endpoint.elog().unset_level(websocketpp::log::elevel::ALL);
        
        con = endpoint.get_connection(uri);

        con->add_subprotocol("sip");

        con->set_origin("http://zaphoyd.com");

        endpoint.connect(con);
                
        endpoint.run();
        
        while(!boost::dynamic_pointer_cast<sip_client_handler>(handler)->received) {
            boost::this_thread::sleep(boost::posix_time::milliseconds(100)); 
        }
        
        std::cout << "done" << std::endl;
        
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    
    return 0;
}
