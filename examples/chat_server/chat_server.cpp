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

#include "chat.hpp"

#include "../../src/websocketpp.hpp"
#include <boost/asio.hpp>

#include <iostream>

using boost::asio::ip::tcp;
using namespace websocketchat;

int main(int argc, char* argv[]) {
	std::string host = "localhost";
	short port = 9003;
	std::string full_host;
	
	if (argc == 3) {
		// TODO: input validation?
		host = argv[1];
		port = atoi(argv[2]);
	}
		
	std::stringstream temp;
	
	temp << host << ":" << port;	
	full_host = temp.str();
	
	
	chat_server_handler_ptr chat_handler(new chat_server_handler());
	
	try {
		boost::asio::io_service io_service;
		tcp::endpoint endpoint(tcp::v6(), port);
		
		websocketpp::server_ptr server(
			new websocketpp::server(io_service,endpoint,chat_handler)
		);
		
		// setup server settings
		server->add_host(host);
		server->add_host(full_host);
		// Chat server should only be receiving small text messages, reduce max
		// message size limit slightly to save memory, improve performance, and 
		// guard against DoS attacks.
		server->set_max_message_size(0xFFFF); // 64KiB
		
		// start the server
		server->start_accept();
		
		std::cout << "Starting chat server on " << full_host << std::endl;
		
		io_service.run();
	} catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
	}
	
	return 0;
}
