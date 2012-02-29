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

#include "../../src/sockets/tls.hpp"
#include "../../src/websocketpp.hpp"

#include <cstring>

using websocketpp::server;
using websocketpp::server_tls;

template <typename endpoint_type>
class echo_server_handler : public endpoint_type::handler {
public:
    typedef echo_server_handler<endpoint_type> type;
    typedef typename endpoint_type::handler::connection_ptr connection_ptr;
    typedef typename endpoint_type::handler::message_ptr message_ptr;
    
    std::string get_password() const {
        return "test";
    }
    
    boost::shared_ptr<boost::asio::ssl::context> on_tls_init() {
        // create a tls context, init, and return.
        boost::shared_ptr<boost::asio::ssl::context> context(new boost::asio::ssl::context(boost::asio::ssl::context::tlsv1));
        try {
            context->set_options(boost::asio::ssl::context::default_workarounds |
                                 boost::asio::ssl::context::no_sslv2 |
                                 boost::asio::ssl::context::single_dh_use);
            context->set_password_callback(boost::bind(&type::get_password, this));
            context->use_certificate_chain_file("../../src/ssl/server.pem");
            context->use_private_key_file("../../src/ssl/server.pem", boost::asio::ssl::context::pem);
            context->use_tmp_dh_file("../../src/ssl/dh512.pem");
        } catch (std::exception& e) {
            std::cout << e.what() << std::endl;
        }
        return context;
    }
    
    void on_message(connection_ptr con,message_ptr msg) {
        con->send(msg->get_payload(),msg->get_opcode());
    }
    
    void http(connection_ptr con) {
        con->set_body("<!DOCTYPE html><html><head><title>WebSocket++ TLS certificate test</title></head><body><h1>WebSocket++ TLS certificate test</h1><p>This is an HTTP(S) page served by a WebSocket++ server for the purposes of confirming that certificates are working since browsers normally silently ignore certificate issues.</p></body></html>");
    }
};

int main(int argc, char* argv[]) {
    unsigned short port = 9002;
    bool tls = false;
        
    if (argc >= 2) {
        port = atoi(argv[1]);
        
        if (port == 0) {
            std::cout << "Unable to parse port input " << argv[1] << std::endl;
            return 1;
        }
    }
    
    if (argc == 3) {
        tls = !strcmp(argv[2],"-tls");
    }
    
    try {
        if (tls) {
            server_tls::handler::ptr handler(new echo_server_handler<server_tls>());
            server_tls endpoint(handler);
            
            endpoint.alog().unset_level(websocketpp::log::alevel::ALL);
            endpoint.elog().unset_level(websocketpp::log::elevel::ALL);
            
            std::cout << "Starting Secure WebSocket echo server on port " 
                      << port << std::endl;
            endpoint.listen(port);
        } else {
            server::handler::ptr handler(new echo_server_handler<server>());
            server endpoint(handler);
            
            endpoint.alog().unset_level(websocketpp::log::alevel::ALL);
            endpoint.elog().unset_level(websocketpp::log::elevel::ALL);
            
            std::cout << "Starting WebSocket echo server on port " 
                      << port << std::endl;
            endpoint.listen(port);
        }
        
        
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    
    return 0;
}
