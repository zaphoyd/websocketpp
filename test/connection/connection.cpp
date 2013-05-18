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
//#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE connection
#include <boost/test/unit_test.hpp>

#include "connection_tu2.hpp"

// NOTE: these tests currently test against hardcoded output values. I am not 
// sure how problematic this will be. If issues arise like order of headers the 
// output should be parsed by http::response and have values checked directly

BOOST_AUTO_TEST_CASE( basic_http_request ) {
    std::string input = "GET / HTTP/1.1\r\nHost: www.example.com\r\n\r\n";
    std::string output = "HTTP/1.1 500 Internal Server Error\r\nServer: " + 
		                 std::string(websocketpp::user_agent)+"\r\n\r\n";
	
	std::string o2 = run_server_test(input);

	std::cout << "output: " << o2 << std::endl;

    BOOST_CHECK(o2 == output);
}

struct connection_extension {
    connection_extension() : extension_value(5) {}
    
    int extension_method() {
        return extension_value;
    }
    
    bool is_server() const {
        return false;
    }
    
    int extension_value;
};

struct stub_config : public websocketpp::config::core {
    typedef core::concurrency_type concurrency_type;

    typedef core::request_type request_type;
    typedef core::response_type response_type;

    typedef core::message_type message_type;
    typedef core::con_msg_manager_type con_msg_manager_type;
    typedef core::endpoint_msg_manager_type endpoint_msg_manager_type;

    typedef core::alog_type alog_type;
    typedef core::elog_type elog_type;

    typedef core::rng_type rng_type;
    
    typedef core::transport_type transport_type;
    
    typedef core::endpoint_base endpoint_base;
    typedef connection_extension connection_base;
};

struct connection_setup {
    connection_setup(bool server) 
      : c(server,"",alog,elog,rng) {}
    
    websocketpp::lib::error_code ec;
	stub_config::alog_type alog;
    stub_config::elog_type elog;
	stub_config::rng_type rng;
	websocketpp::connection<stub_config> c;
};

/*void echo_func(server* s, websocketpp::connection_hdl hdl, message_ptr msg) {
    s->send(hdl, msg->get_payload(), msg->get_opcode());
}*/

void validate_func(server* s, websocketpp::connection_hdl hdl, message_ptr msg) {
    s->send(hdl, msg->get_payload(), msg->get_opcode());
}

void http_func(server* s, websocketpp::connection_hdl hdl) {
    server::connection_ptr con = s->get_con_from_hdl(hdl);
    
    std::string res = con->get_resource();
    
    con->set_body(res);
    con->set_status(websocketpp::http::status_code::ok);
}

BOOST_AUTO_TEST_CASE( connection_extensions ) {
    connection_setup env(true);
    
    BOOST_CHECK( env.c.extension_value == 5 );
    BOOST_CHECK( env.c.extension_method() == 5 );
    
    BOOST_CHECK( env.c.is_server() == true );
}

BOOST_AUTO_TEST_CASE( basic_websocket_request ) {
    std::string input = "GET / HTTP/1.1\r\nHost: www.example.com\r\nConnection: upgrade\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nOrigin: http://www.example.com\r\n\r\n";
    std::string output = "HTTP/1.1 101 Switching Protocols\r\nConnection: upgrade\r\nSec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\nServer: ";
    output+=websocketpp::user_agent;
    output+="\r\nUpgrade: websocket\r\n\r\n";
	
	server s;
	s.set_message_handler(bind(&echo_func,&s,::_1,::_2));
	
    BOOST_CHECK(run_server_test(s,input) == output);
}

BOOST_AUTO_TEST_CASE( http_request ) {
    std::string input = "GET /foo/bar HTTP/1.1\r\nHost: www.example.com\r\nOrigin: http://www.example.com\r\n\r\n";
    std::string output = "HTTP/1.1 200 OK\r\nContent-Length: 8\r\nServer: ";
    output+=websocketpp::user_agent;
    output+="\r\n\r\n/foo/bar";
	
	server s;
	s.set_http_handler(bind(&http_func,&s,::_1));
	
    BOOST_CHECK_EQUAL(run_server_test(s,input), output);
}

/*

BOOST_AUTO_TEST_CASE( user_reject_origin ) {
    std::string input = "GET / HTTP/1.1\r\nHost: www.example.com\r\nConnection: upgrade\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nOrigin: http://www.example2.com\r\n\r\n";
    std::string output = "HTTP/1.1 403 Forbidden\r\nServer: "+websocketpp::USER_AGENT+"\r\n\r\n";
	    
    BOOST_CHECK(run_server_test(input) == output);
}

BOOST_AUTO_TEST_CASE( basic_text_message ) {
    std::string input = "GET / HTTP/1.1\r\nHost: www.example.com\r\nConnection: upgrade\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nOrigin: http://www.example.com\r\n\r\n";
    
	unsigned char frames[8] = {0x82,0x82,0xFF,0xFF,0xFF,0xFF,0xD5,0xD5};
	input.append(reinterpret_cast<char*>(frames),8);
	
	std::string output = "HTTP/1.1 101 Switching Protocols\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\nServer: "+websocketpp::USER_AGENT+"\r\nUpgrade: websocket\r\n\r\n**";

    BOOST_CHECK( run_server_test(input) == output);
}
*/
