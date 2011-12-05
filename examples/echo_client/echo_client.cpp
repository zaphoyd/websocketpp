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
	
	void validate(connection_ptr connection) {
		//std::cout << "state: " << connection->get_state() << std::endl;
	}
	
	void on_open(connection_ptr connection) {
		//std::cout << "connection opened" << std::endl;
	}
	
	void on_close(connection_ptr connection) {
		//std::cout << "connection closed" << std::endl;
	}
	
	void on_message(connection_ptr connection,websocketpp::message::data_ptr msg) {
		//std::cout << "got message: " << *msg << std::endl;
		connection->send(msg->get_payload(),(msg->get_opcode() == websocketpp::frame::opcode::BINARY));
		
		
		
		if (connection->get_resource() == "/getCaseCount") {
			std::cout << "Detected " << msg.get_payload() << " test cases." << std::endl;
			m_case_count = atoi(msg.get_payload().c_str());
		} else {
			connection->send(msg->get_payload(),(msg->get_opcode() == websocketpp::frame::opcode::BINARY));
		}
		
		
		connection->recycle(msg);
	}
	
	void http(connection_ptr connection) {
		connection->set_body("HTTP Response!!");
	}
	
	void on_fail(connection_ptr connection) {
		std::cout << "connection failed" << std::endl;
	}
	
	int m_case_count;
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
		
		connection = endpoint.connect("ws://localhost:9001/getCaseCount");
		
        
        
        
		/*boost::asio::io_service io_service;
		
		websocketpp::client_ptr client(new websocketpp::client(io_service,c));
		
		client->init();
		client->set_header("User Agent","WebSocket++/2011-10-27");
		
		client->connect("ws://localhost:9001/getCaseCount");
		io_service.run();*/
		
		std::cout << "case count: " << c->m_case_count << std::endl;
		
		for (int i = 1; i <= c->m_case_count; i++) {
			std::stringstream url;
			
			url << "ws://localhost:9001/runCase?case=" << i << "&agent=\"WebSocket++Snapshot/2011-10-27\"";
						
            connection = endpoint.connect(url.str());
		}
		
		std::cout << "done" << std::endl;
		
	} catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
	}
	
	return 0;
}
