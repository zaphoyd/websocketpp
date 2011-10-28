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

#include "echo_client_handler.hpp"

#include "../../src/websocketpp.hpp"
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <iostream>

using boost::asio::ip::tcp;
using namespace websocketecho;

int main(int argc, char* argv[]) {
	std::string uri;
	
	/*if (argc != 2) {
		std::cout << "Usage: `echo_client test_url`" << std::endl;
	} else {
		uri = argv[1];
	}*/
	
	echo_client_handler_ptr c(new echo_client_handler());
	
	try {
		boost::asio::io_service io_service;
		
		websocketpp::client_ptr client(new websocketpp::client(io_service,c));
		
		client->init();
		client->set_header("User Agent","WebSocket++/2011-10-27");
		
		client->connect("ws://localhost:9001/getCaseCount");
		io_service.run();
		
		std::cout << "case count: " << c->m_case_count << std::endl;
		
		for (int i = 1; i <= c->m_case_count; i++) {
			io_service.reset();
						
			client->set_alog_level(websocketpp::ALOG_OFF);
			client->set_elog_level(websocketpp::LOG_OFF);
			
			client->init();
			client->set_header("User Agent","WebSocket++/2011-10-27");
			
			
			std::stringstream url;
			
			url << "ws://localhost:9001/runCase?case=" << i << "&agent=\"WebSocket++Snapshot/2011-10-27\"";
			
			client->connect(url.str());
			
			io_service.run();
		}
		
		std::cout << "done" << std::endl;
		
	} catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
	}
	
	return 0;
}
