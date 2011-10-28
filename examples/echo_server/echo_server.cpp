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

#include "echo.hpp"

#include "../../src/websocketpp.hpp"
#include <boost/asio.hpp>

#include <iostream>

using boost::asio::ip::tcp;

int main(int argc, char* argv[]) {
	std::string host = "localhost";
	short port = 9002;
	std::string full_host;
	
	if (argc == 3) {
		// TODO: input validation?
		host = argv[1];
		port = atoi(argv[2]);
	}
	
	std::stringstream temp;
	
	temp << host << ":" << port;
	full_host = temp.str();
	
	websocketecho::echo_server_handler_ptr echo_handler(new websocketecho::echo_server_handler());
	
	try {
		boost::asio::io_service io_service;
		tcp::endpoint endpoint(tcp::v6(), port);
		
		websocketpp::server_ptr server(
			new websocketpp::server(io_service,endpoint,echo_handler)
		);
		
		//server->parse_command_line(argc, argv);
		
		
		
		// setup server settings
		//server->set_alog_level(websocketpp::ALOG_OFF);
		//server->set_elog_level(websocketpp::LOG_OFF);
		
		server->add_host(host);
		server->add_host(full_host);
		
		// bump up max message size to maximum since we may be using the echo 
		// server to test performance and protocol extremes.
		server->set_max_message_size(websocketpp::frame::PAYLOAD_64BIT_LIMIT); 
		
		// start the server
		server->start_accept();
		
		std::cout << "Starting echo server on " << full_host << std::endl;
		
		// start asio
		io_service.run();
	} catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
	}
	
	return 0;
}
