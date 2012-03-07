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
#include <sstream>

using websocketpp::server;

// A request encapsulates all of the information necesssary to perform a request
// the coordinator will fill in this information from the websocket connection 
// and add it to the processing queue. Sleeping in this example is a placeholder
// for any long serial task.
struct request {
    server::handler::connection_ptr con;
    int                             value;
    
    void process() {
        std::stringstream response;
        response << "Sleeping for " << value << " milliseconds!";
        con->send(response.str());
        
        boost::this_thread::sleep(boost::posix_time::milliseconds(value));
        
        response.str("");
        response << "Done sleeping for " << value << " milliseconds!";
        con->send(response.str());
    }
};

// The coordinator is a simple wrapper around an STL queue. add_request inserts
// a new request. get_request returns the next available request and blocks
// (using condition variables) in the case that the queue is empty.
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

// The WebSocket++ handler in this case reads numbers from connections and packs
// connection pointer + number into a request struct and passes it off to the
// coordinator.
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

class server_handler : public server::handler {
public:    
    void on_message(connection_ptr con,message_ptr msg) {
        int value = atoi(msg->get_payload().c_str());
        
        if (value == 0) {
            con->send("Invalid sleep value.");
        } else {
            request r;
            r.con = con;
            r.value = value;
            r.process();
        }
    }
};

// process_requests is the body function for a processing thread. It loops 
// forever reading requests, processing them serially, then reading another 
// request. 
void process_requests(request_coordinator* coordinator);

void process_requests(request_coordinator* coordinator) {
    request r;
    
    while (1) {
        coordinator->get_request(r);
        
        r.process();
    }
}

// concurrent server takes two arguments. A port to bind to and a number of 
// worker threads to create. The thread count must be an integer greater than
// or equal to zero.
// 
// num_threads=0 Standard non-threaded WebSocket++ mode. Handlers will block
//               i/o operations and other handlers.
// num_threads=1 One thread processes requests serially the other handles i/o
//               This allows new connections and requests to be made while the
//               processing thread is busy, but does allow long jobs to 
//               monopolize the processor increasing request latency.
// num_threads>1 Multiple processing threads will work on the single queue of
//               requests provided by the i/o thread. This enables out of order
//               completion of requests. The number of threads can be tuned 
//               based on hardware concurrency available and expected load and
//               job length.
int main(int argc, char* argv[]) {
    unsigned short port = 9002;
    unsigned short num_threads = 2;
    
    try {
        if (argc == 2) {
            std::stringstream buffer(argv[1]);
            buffer >> port;
        }
        
        if (argc == 3) {
            std::stringstream buffer(argv[2]);
            buffer >> num_threads;
        }
            
        request_coordinator rc;
        
        server::handler::ptr h;
        if (num_threads == 0) {
            h = server::handler::ptr(new server_handler());
        } else {
            h = server::handler::ptr(new concurrent_server_handler(rc));
        }
        
        server echo_endpoint(h);
        
        echo_endpoint.alog().unset_level(websocketpp::log::alevel::ALL);
        echo_endpoint.elog().unset_level(websocketpp::log::elevel::ALL);
        
        echo_endpoint.elog().set_level(websocketpp::log::elevel::RERROR);
        echo_endpoint.elog().set_level(websocketpp::log::elevel::FATAL);
        
        std::list<boost::shared_ptr<boost::thread> > threads;
        
        for (int i = 0; i < num_threads; i++) {
            threads.push_back(boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&process_requests, &rc))));
        }
        
        std::cout << "Starting WebSocket sleep server on port " << port << " with " << num_threads << " processing threads." << std::endl;
        echo_endpoint.listen(port);
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    
    return 0;
}
