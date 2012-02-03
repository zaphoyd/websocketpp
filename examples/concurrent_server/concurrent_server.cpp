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

#include "../../src/websocketpp.hpp"

#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

#include <cstring>

using websocketpp::server;

struct request {
    server::handler::connection_ptr con;
    int                             value;
};

class request_coordinator {
public:
    void add_request(const request& r) {
        boost::unique_lock<boost::mutex> lock(m_lock);
        m_requests.push(r);
        lock.unlock();
        m_cond.notify_one();
    }
    
    void get_request(request& value) {
        boost::unique_lock<boost::mutex> lock(m_lock);
        
        while (m_requests.empty()) {
            m_cond.wait(lock);
        }
        
        value = m_requests.front();
        m_requests.pop();
    }
private:
    std::queue<request>         m_requests;
    boost::mutex                m_lock;
    boost::condition_variable   m_cond;
};

class concurrent_server_handler : public server::handler {
public:
    concurrent_server_handler(request_coordinator& c) : m_coordinator(c) {}
    
    void on_message(connection_ptr con,message_ptr msg) {
        int value = atoi(msg->get_payload().c_str());
        
        if (value == 0) {
            con->send("Invalid sleep value.");
        } else {
            request r;
            r.con = con;
            r.value = value;
            m_coordinator.add_request(r);
        }
    }
private:
    request_coordinator& m_coordinator;
};



void process_requests(request_coordinator* coordinator);

void process_requests(request_coordinator* coordinator) {
    request r;
    
    while (1) {
        coordinator->get_request(r);
        
        std::stringstream response;
        response << "Sleeping for " << r.value << " milliseconds!";
        r.con->send(response.str());
        
        boost::this_thread::sleep(boost::posix_time::milliseconds(r.value));
        
        response.str("");
        response << "Done sleeping for " << r.value << " milliseconds!";
        r.con->send(response.str());
    }
}

int main(int argc, char* argv[]) {
    unsigned short port = 9002;
        
    if (argc == 2) {
        port = atoi(argv[1]);
        
        if (port == 0) {
            std::cout << "Unable to parse port input " << argv[1] << std::endl;
            return 1;
        }
    }
    
    try {       
        request_coordinator rc;
        
        server::handler::ptr h(new concurrent_server_handler(rc));
        server echo_endpoint(h);
        
        echo_endpoint.alog().unset_level(websocketpp::log::alevel::ALL);
        echo_endpoint.elog().unset_level(websocketpp::log::elevel::ALL);
        
        echo_endpoint.elog().set_level(websocketpp::log::elevel::ERROR);
        echo_endpoint.elog().set_level(websocketpp::log::elevel::FATAL);
        
        boost::thread t1(boost::bind(&process_requests, &rc));
        boost::thread t2(boost::bind(&process_requests, &rc));
        
        std::cout << "Starting WebSocket sleep server on port " << port << std::endl;
        echo_endpoint.listen(port);
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    
    return 0;
}
