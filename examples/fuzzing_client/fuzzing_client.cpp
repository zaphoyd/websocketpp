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
#include "../../src/roles/client.hpp"

#include <iostream>

typedef websocketpp::endpoint<websocketpp::role::client,websocketpp::socket::plain> plain_endpoint_type;
typedef plain_endpoint_type::handler_ptr plain_handler_ptr;
typedef plain_endpoint_type::connection_ptr connection_ptr;

class echo_client_handler : public plain_endpoint_type::handler {
public:
	typedef echo_client_handler type;
	typedef plain_endpoint_type::connection_ptr connection_ptr;
	
	void on_open(connection_ptr connection) {
		agent = connection->get_response_header("Server");
		std::cout << "Testing agent " << agent << std::endl;
		
		// needs more C++11 intializer lists =(
		test_9_1_X_sizes[0] = 65536;
		test_9_1_X_sizes[1] = 262144;
		test_9_1_X_sizes[2] = 262144;
		test_9_1_X_sizes[3] = 4194304;
		test_9_1_X_sizes[4] = 8388608;
		test_9_1_X_sizes[5] = 16777216;
		
		test = 1;
		
		
		start_9_1_X(test);
		connection->send(data,true);
	}
	
	void on_message(connection_ptr connection,websocketpp::message::data_ptr msg) {		
		end_time = boost::posix_time::microsec_clock::local_time();
		
		boost::posix_time::time_period len(start_time,end_time);
		
		if (msg->get_payload() == data) {
			std::cout << " passes in " << len.length() << std::endl;
		} else {
			std::cout << " fails in " << len.length() << std::endl;
		}
		
		connection->recycle(msg);
		
		if (test == 6) {
			std::cout << "Done" << std::endl;
			connection->close(websocketpp::close::status::NORMAL, "");
		} else {
			test++;
			start_9_1_X(test);
			connection->send(data,true);
		}
	}
	
	void http(connection_ptr connection) {
		//connection->set_body("HTTP Response!!");
	}
	
	void on_fail(connection_ptr connection) {
		std::cout << "connection failed" << std::endl;
	}
	
	void start_9_1_X(int subtest) {
		std::cout << "Test 9.2." << subtest << " ";
		
		data.reserve(test_9_1_X_sizes[subtest-1]);
		data.assign(test_9_1_X_sizes[subtest-1],'*');
		start_time = boost::posix_time::microsec_clock::local_time();
	}
	
	size_t	test_9_1_X_sizes[6];
	int			test;
	std::string agent;
	std::string data;
	boost::posix_time::ptime start_time;
	boost::posix_time::ptime end_time;
};


int main(int argc, char* argv[]) {
	std::string uri;
	
	/*if (argc != 2) {
		std::cout << "Usage: `echo_client test_url`" << std::endl;
	} else {
		uri = argv[1];
	}*/
	
	try {
		plain_handler_ptr handler(new echo_client_handler());
        connection_ptr connection;
		plain_endpoint_type endpoint(handler);
		
		endpoint.alog().unset_level(websocketpp::log::alevel::ALL);
		
		endpoint.elog().unset_level(websocketpp::log::elevel::ALL);
		
		connection = endpoint.connect("ws://localhost:9000/");
		
        endpoint.run();
        
		std::cout << "done" << std::endl;
		
	} catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
	}
	
	return 0;
}
