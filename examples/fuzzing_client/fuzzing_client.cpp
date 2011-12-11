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

// base test case handler
class test_case_handler : public plain_endpoint_type::handler {
public:
	typedef test_case_handler type;
	typedef plain_endpoint_type::connection_ptr connection_ptr;
		
	virtual void end(connection_ptr connection) {
		// test is over, give control back to controlling handler
		boost::posix_time::time_period len(m_start_time,m_end_time);
		
		if (m_pass) {
			std::cout << " passes in " << len.length() << std::endl;
		} else {
			std::cout << " fails in " << len.length() << std::endl;
		}
		
		connection->close(websocketpp::close::status::NORMAL,"");
	}
protected:
	bool						m_pass;
	boost::posix_time::ptime	m_start_time;
	boost::posix_time::ptime	m_end_time;
};

// test class for 9.1.* and 9.2.*
class test_9_1_X : public test_case_handler {
public:	
	test_9_1_X(int minor, int subtest) : m_minor(minor),m_subtest(subtest){
		// needs more C++11 intializer lists =(
		m_test_sizes[0] = 65536;
		m_test_sizes[1] = 262144;
		m_test_sizes[2] = 1048576;
		m_test_sizes[3] = 4194304;
		m_test_sizes[4] = 8388608;
		m_test_sizes[5] = 16777216;
	}
	
	void on_open(connection_ptr connection) {
		std::cout << "Test 9." << m_minor << "." << m_subtest;
		
		
		
		m_data.reserve(m_test_sizes[m_subtest-1]);
		
		if (m_minor == 1) {
			fill_utf8(connection,true);
			m_start_time = boost::posix_time::microsec_clock::local_time();
			connection->send(m_data,false);
		} else if (m_minor == 2) {
			fill_binary(connection,true);
			m_start_time = boost::posix_time::microsec_clock::local_time();
			connection->send(m_data,true);
		} else {
			std::cout << " has unknown definition." << std::endl;
		}
	}
	
	// Just does random ascii right now. True random UTF8 with multi-byte stuff
	// would probably be better
	void fill_utf8(connection_ptr connection,bool random = true) {
		if (random) {
			uint32_t data;
			for (int i = 0; i < m_test_sizes[m_subtest-1]; i++) {
				if (i%4 == 0) {
					data = uint32_t(connection->rand());
				}
				
				m_data.push_back(char(((reinterpret_cast<uint8_t*>(&data)[i%4])%95)+32));
			}
		} else {
			m_data.assign(m_test_sizes[m_subtest-1],'*');
		}
	}
	
	void fill_binary(connection_ptr connection, bool random = true) {
		if (random) {
			int32_t data;
			for (int i = 0; i < m_test_sizes[m_subtest-1]; i++) {
				if (i%4 == 0) {
					data = connection->rand();
				}
				
				m_data.push_back((reinterpret_cast<char*>(&data))[i%4]);
			}
		} else {
			m_data.assign(m_test_sizes[m_subtest-1],'*');
		}
	}
	
	void on_message(connection_ptr connection,websocketpp::message::data_ptr msg) {
		m_end_time = boost::posix_time::microsec_clock::local_time();
		
		// Check whether the echoed data matches exactly
		m_pass = (msg->get_payload() == m_data);
		
		connection->recycle(msg);
		this->end(connection);
	}
private:
	int			m_minor;
	int			m_subtest;
	size_t		m_test_sizes[6];
	std::string m_data;
};

// test class for 9.7.* and 9.8.*
/*class test_9_7_X : public test_case_handler {
public:	
	test_9_1_X(int minor, int subtest) : m_minor(minor),m_subtest(subtest){
		// needs more C++11 intializer lists =(
		m_test_sizes[0] = 65536;
		m_test_sizes[1] = 262144;
		m_test_sizes[2] = 1048576;
		m_test_sizes[3] = 4194304;
		m_test_sizes[4] = 8388608;
		m_test_sizes[5] = 16777216;
	}
	
	void on_open(connection_ptr connection) {
		std::cout << "Test 9." << m_minor << "." << m_subtest;
		
		
		
		m_data.reserve(m_test_sizes[m_subtest-1]);
		
		if (m_minor == 1) {
			fill_utf8(connection,true);
			m_start_time = boost::posix_time::microsec_clock::local_time();
			connection->send(m_data,false);
		} else if (m_minor == 2) {
			fill_binary(connection,true);
			m_start_time = boost::posix_time::microsec_clock::local_time();
			connection->send(m_data,true);
		} else {
			std::cout << " has unknown definition." << std::endl;
		}
	}
	
	// Just does random ascii right now. True random UTF8 with multi-byte stuff
	// would probably be better
	void fill_utf8(connection_ptr connection,bool random = true) {
		if (random) {
			uint32_t data;
			for (int i = 0; i < m_test_sizes[m_subtest-1]; i++) {
				if (i%4 == 0) {
					data = uint32_t(connection->rand());
				}
				
				m_data.push_back(char(((reinterpret_cast<uint8_t*>(&data)[i%4])%95)+32));
			}
		} else {
			m_data.assign(m_test_sizes[m_subtest-1],'*');
		}
	}
	
	void fill_binary(connection_ptr connection, bool random = true) {
		if (random) {
			int32_t data;
			for (int i = 0; i < m_test_sizes[m_subtest-1]; i++) {
				if (i%4 == 0) {
					data = connection->rand();
				}
				
				m_data.push_back((reinterpret_cast<char*>(&data))[i%4]);
			}
		} else {
			m_data.assign(m_test_sizes[m_subtest-1],'*');
		}
	}
	
	void on_message(connection_ptr connection,websocketpp::message::data_ptr msg) {
		m_end_time = boost::posix_time::microsec_clock::local_time();
		
		// Check whether the echoed data matches exactly
		m_pass = (msg->get_payload() == m_data);
		
		connection->recycle(msg);
		this->end(connection);
	}
private:
	int			m_minor;
	int			m_subtest;
	size_t		m_test_sizes[6];
	std::string m_data;
};*/

int main(int argc, char* argv[]) {
	std::string uri;
	
	
	
	if (argc != 2) {
		uri = "ws://localhost:9002/";
	} else {
		uri = argv[1];
	}
	
	std::vector<plain_handler_ptr>	tests;
	
	for (int i = 1; i <= 2; i++) {
		for (int j = 1; j <= 6; j++) {
			tests.push_back(plain_handler_ptr(new test_9_1_X(i,j)));
		}
	}
	
	try {
		plain_endpoint_type endpoint(tests[0]);
		
		endpoint.alog().unset_level(websocketpp::log::alevel::ALL);
		endpoint.elog().unset_level(websocketpp::log::elevel::ALL);
		
		for (int i = 0; i < tests.size(); i++) {
			if (i > 0) {
				endpoint.reset();
				endpoint.set_handler(tests[i]);
			}
			
			endpoint.connect(uri);
			endpoint.run();
		}
        
		std::cout << "done" << std::endl;
		
	} catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
	}
	
	return 0;
}
