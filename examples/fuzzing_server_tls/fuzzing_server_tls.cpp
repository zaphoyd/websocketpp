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

#include "../../src/endpoint.hpp"
#include "../../src/roles/server.hpp"
#include "../../src/sockets/ssl.hpp"

#include <boost/date_time/posix_time/posix_time.hpp>

#include <cstring>

typedef websocketpp::endpoint<websocketpp::role::server,websocketpp::socket::plain> plain_endpoint_type;
typedef websocketpp::endpoint<websocketpp::role::server,websocketpp::socket::ssl> tls_endpoint_type;
typedef plain_endpoint_type::handler_ptr plain_handler_ptr;
typedef tls_endpoint_type::handler_ptr tls_handler_ptr;

template <typename endpoint_type>
class echo_server_handler : public endpoint_type::handler {
public:
    typedef echo_server_handler<endpoint_type> type;
    typedef typename endpoint_type::connection_ptr connection_ptr;
    
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
            context->use_certificate_chain_file("/Users/zaphoyd/Documents/websocketpp/src/ssl/server.pem");
            context->use_private_key_file("/Users/zaphoyd/Documents/websocketpp/src/ssl/server.pem", boost::asio::ssl::context::pem);
            context->use_tmp_dh_file("/Users/zaphoyd/Documents/websocketpp/src/ssl/dh512.pem");
        } catch (std::exception& e) {
            std::cout << e.what() << std::endl;
        }
        return context;
    }
    
    void validate(connection_ptr connection) {
        //std::cout << "state: " << connection->get_state() << std::endl;
        
        
        
        
    }
    
    void on_open(connection_ptr connection) {
        //std::cout << "connection opened" << std::endl;
        // extract user agent
        // start timer
        start_time = boost::posix_time::microsec_clock::local_time();
        // send message
        connection->send("abcd");
        // stop
    }
    
    void on_close(connection_ptr connection) {
        //std::cout << "connection closed" << std::endl;
    }
    
    void on_message(connection_ptr connection,websocketpp::message::data_ptr msg) {
        //std::cout << "got message: " << *msg << std::endl;
        //connection->send(msg->get_payload(),(msg->get_opcode() == websocketpp::frame::opcode::BINARY));
        
        end_time = boost::posix_time::microsec_clock::local_time();
        
        boost::posix_time::time_period len(start_time,end_time);
        
        if (msg->get_payload() == "abcd") {
            std::cout << "Pass in " << len.length() << std::endl;
        } else {
            std::cout << "Fail in " << len.length() << std::endl;
        }
        
        // stop timer
        // check if message was valid
    }
    
    void http(connection_ptr connection) {
        connection->set_body("HTTP Response!!");
    }
    
    void on_fail(connection_ptr connection) {
        std::cout << "connection failed" << std::endl;
    }
    
    boost::posix_time::ptime start_time;
    boost::posix_time::ptime end_time;
};

int main(int argc, char* argv[]) {
    unsigned short port = 9002;
    bool tls = false;
        
    if (argc == 2) {
        // TODO: input validation?
        port = atoi(argv[1]);
    }
    
    if (argc == 3) {
        // TODO: input validation?
        port = atoi(argv[1]);
        tls = !strcmp(argv[2],"-tls");
    }
    
    try {
        if (tls) {
            tls_handler_ptr h(new echo_server_handler<tls_endpoint_type>());
            tls_endpoint_type e(h);
            
            e.alog().unset_level(websocketpp::log::alevel::ALL);
            //e.alog().set_level(websocketpp::log::alevel::CONNECT);
            //e.alog().set_level(websocketpp::log::alevel::DISCONNECT);
            //e.alog().set_level(websocketpp::log::alevel::DEVEL);
            //e.alog().set_level(websocketpp::log::alevel::DEBUG_CLOSE);
            //e.alog().unset_level(websocketpp::log::alevel::DEBUG_HANDSHAKE);
            
            e.elog().unset_level(websocketpp::log::elevel::ALL);
            //e.elog().set_level(websocketpp::log::elevel::ERROR);
            //e.elog().set_level(websocketpp::log::elevel::FATAL);
            
            std::cout << "Starting Secure WebSocket echo server on port " << port << std::endl;
            e.listen(port);
        } else {
            plain_handler_ptr h(new echo_server_handler<plain_endpoint_type>());
            plain_endpoint_type e(h);
            
            e.alog().unset_level(websocketpp::log::alevel::ALL);
            //e.alog().set_level(websocketpp::log::alevel::CONNECT);
            //e.alog().set_level(websocketpp::log::alevel::DISCONNECT);
            //e.alog().unset_level(websocketpp::log::alevel::DEBUG_HANDSHAKE);
            
            e.elog().unset_level(websocketpp::log::elevel::ALL);
            //e.elog().set_level(websocketpp::log::elevel::ERROR);
            //e.elog().set_level(websocketpp::log::elevel::FATAL);
            
            // TODO: fix
            //e.alog().set_level(websocketpp::log::alevel::CONNECT & websocketpp::log::alevel::DISCONNECT);
            //e.elog().set_levels(websocketpp::log::elevel::ERROR,websocketpp::log::elevel::FATAL);
            
            std::cout << "Starting WebSocket echo server on port " << port << std::endl;
            e.listen(port);
        }
        
        
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    
    return 0;
}
