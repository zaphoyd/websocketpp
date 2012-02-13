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

#include <boost/date_time/posix_time/posix_time.hpp>

#include <cstring>

using websocketpp::server;
using websocketpp::server_tls;

template <typename endpoint_type>
class fuzzing_server_handler : public endpoint_type::handler {
public:
    typedef fuzzing_server_handler<endpoint_type> type;
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
            context->use_private_key_file("../..p/src/ssl/server.pem", boost::asio::ssl::context::pem);
            context->use_tmp_dh_file("../../src/ssl/dh512.pem");
        } catch (std::exception& e) {
            std::cout << e.what() << std::endl;
        }
        return context;
    }
    
    void validate(connection_ptr con) {
        //std::cout << "state: " << connection->get_state() << std::endl;
    }
    
    void on_open(connection_ptr con) {
        if (con->get_resource() == "/getCaseCount") {
            con->send("12");
            con->close(websocketpp::close::status::NORMAL);
            return;
        }
                
        if (con->get_resource().find("/runCase?case=") == 0) {
            std::string foo = con->get_resource();
            
            size_t a = foo.find("case=");
            size_t b = foo.find("&");
            size_t c = foo.find("agent=");
            
            std::string case_no = foo.substr(a+5,b-(a+5));
            std::string agent = foo.substr(c+6,foo.size()-(c+6));
            
            // /runCase?case=" << i << "&agent=\"WebSocket++/0.2.0\"
            
            m_case_no = std::atoi(case_no.c_str());
            
            if (m_case_no == 1) {
                std::cout << "Running tests for agent: " << agent << std::endl;
            }
        } else {
            std::cout << "Running tests for agent: Unknown" << std::endl;
            m_case_no = 12;
        }
        
        
        
        //std::cout << "connection opened" << std::endl;
        // extract user agent
        // start timer
        
        // send message
        
        m_test_sizes[0] = 65536;
        m_test_sizes[1] = 262144;
        m_test_sizes[2] = 1048576;
        m_test_sizes[3] = 4194304;
        m_test_sizes[4] = 8388608;
        m_test_sizes[5] = 16777216;
        
        m_data.clear();
        
        websocketpp::frame::opcode::value mode;
        if (m_case_no <= 6) {
            fill_utf8(m_data,m_test_sizes[(m_case_no-1)%6]);
            mode = websocketpp::frame::opcode::TEXT;
        } else {
            fill_binary(m_data,m_test_sizes[(m_case_no-1)%6]);
            mode = websocketpp::frame::opcode::BINARY;
        }
        
        start_time = boost::posix_time::microsec_clock::local_time();
                
        con->send(m_data,mode);
        // stop
    }
    
    void on_close(connection_ptr con) {
        //std::cout << "connection closed" << std::endl;
    }
    
    void on_message(connection_ptr con,message_ptr msg) {
        //std::cout << "got message: " << *msg << std::endl;
        //connection->send(msg->get_payload(),(msg->get_opcode() == websocketpp::frame::opcode::BINARY));
        
        end_time = boost::posix_time::microsec_clock::local_time();
        
        boost::posix_time::time_period len(start_time,end_time);
        
        if (m_case_no <= 6) {
            std::cout << "9.1." << (m_case_no);
        } else {
            std::cout << "9.2." << (m_case_no-6);
        }
        
        if (msg->get_payload() == m_data) {
            std::cout << " Pass in " << len.length() << std::endl;
        } else {
            std::cout << " Fail in " << len.length() << std::endl;
        }
        con->close(websocketpp::close::status::NORMAL);
        // stop timer
        // check if message was valid
    }
    
    void http(connection_ptr connection) {
        connection->set_body("HTTP Response!!");
    }
    
    void on_fail(connection_ptr connection) {
        std::cout << "connection failed" << std::endl;
    }
    
    void fill_utf8(std::string& data,size_t size,bool random = true) {
        if (random) {
            uint32_t val;
            for (int i = 0; i < size; i++) {
                if (i%4 == 0) {
                    val = uint32_t(rand());
                }
                
                data.push_back(char(((reinterpret_cast<uint8_t*>(&val)[i%4])%95)+32));
            }
        } else {
            data.assign(size,'*');
        }
    }
    
    void fill_binary(std::string& data,size_t size,bool random = true) {
        if (random) {
            int32_t val;
            for (int i = 0; i < size; i++) {
                if (i%4 == 0) {
                    val = rand();
                }
                
                data.push_back((reinterpret_cast<char*>(&val))[i%4]);
            }
        } else {
            data.assign(size,'*');
        }
    }
    
    boost::posix_time::ptime start_time;
    boost::posix_time::ptime end_time;
    
    int         m_case_no;
    int         m_minor;
    int         m_subtest;
    int         m_test_sizes[6];
    std::string m_data;
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
            server_tls::handler::ptr handler(new fuzzing_server_handler<server_tls>());
            server_tls endpoint(handler);
            
            endpoint.alog().unset_level(websocketpp::log::alevel::ALL);
            endpoint.elog().unset_level(websocketpp::log::elevel::ALL);
            
            std::cout << "Starting Secure WebSocket fuzzing server on port " 
                      << port << std::endl;
            endpoint.listen(port);
        } else {
            server::handler::ptr handler(new fuzzing_server_handler<server>());
            server endpoint(handler);
            
            endpoint.alog().unset_level(websocketpp::log::alevel::ALL);
            endpoint.elog().unset_level(websocketpp::log::elevel::ALL);
            
            std::cout << "Starting WebSocket fuzzing server on port " 
            << port << std::endl;
            endpoint.listen(port);
        }
        
        
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    
    return 0;
}
