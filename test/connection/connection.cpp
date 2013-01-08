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

#include <iostream>
#include <sstream>

// Test Environment:
// server, no TLS, no locks, iostream based transport
#include <websocketpp/config/core.hpp>
#include <websocketpp/server.hpp>

typedef websocketpp::server<websocketpp::config::core> server;
typedef websocketpp::config::core::message_type::ptr message_ptr;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

/*class echo_handler : public server::handler {
	bool validate(connection_ptr con) {
		std::cout << "handler validate" << std::endl;
		if (con->get_origin() != "http://www.example.com") {
		    con->set_status(websocketpp::http::status_code::FORBIDDEN);
		    return false;
		}
		return true;
	}
};*/

void on_message(server* s, websocketpp::connection_hdl hdl, message_ptr msg) {
    s->send(hdl, msg->get_payload(), msg->get_opcode());
}

std::string run_server_test(std::string input) {
    server test_server;
    server::connection_ptr con;
    
    test_server.set_message_handler(bind(&on_message,&test_server,::_1,::_2));

    std::stringstream output;
	
	test_server.register_ostream(&output);
	
	con = test_server.get_connection();
	
	con->start();
	
	std::stringstream channel;
	
	channel << input;
	channel >> *con;
	
	return output.str();
}

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
/*
BOOST_AUTO_TEST_CASE( basic_websocket_request ) {
    std::string input = "GET / HTTP/1.1\r\nHost: www.example.com\r\nConnection: upgrade\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nOrigin: http://www.example.com\r\n\r\n";
    std::string output = "HTTP/1.1 101 Switching Protocols\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\nServer: "+websocketpp::USER_AGENT+"\r\nUpgrade: websocket\r\n\r\n";
	    
    BOOST_CHECK(run_server_test(input) == output);
}

BOOST_AUTO_TEST_CASE( invalid_websocket_version ) {
    std::string input = "GET / HTTP/1.1\r\nHost: www.example.com\r\nConnection: upgrade\r\nUpgrade: websocket\r\nSec-WebSocket-Version: a\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nOrigin: http://www.example.com\r\n\r\n";
    std::string output = "HTTP/1.1 400 Bad Request\r\nServer: "+websocketpp::USER_AGENT+"\r\n\r\n";
	    
    BOOST_CHECK(run_server_test(input) == output);
}

BOOST_AUTO_TEST_CASE( unimplimented_websocket_version ) {
    std::string input = "GET / HTTP/1.1\r\nHost: www.example.com\r\nConnection: upgrade\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 14\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nOrigin: http://www.example.com\r\n\r\n";

    std::string output = "HTTP/1.1 400 Bad Request\r\nSec-WebSocket-Version: 0,7,8,13\r\nServer: "+websocketpp::USER_AGENT+"\r\n\r\n";
	    
    BOOST_CHECK(run_server_test(input) == output);
}

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
