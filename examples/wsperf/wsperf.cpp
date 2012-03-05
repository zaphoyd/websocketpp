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

#include "wscmd.hpp"
#include "case.hpp"
#include "autobahn.hpp"
#include "generic.hpp"

#include "../../src/roles/client.hpp"
#include "../../src/websocketpp.hpp"

#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

#include <cstring>
#include <sstream>

using websocketpp::server;
using websocketpp::client;

// A request encapsulates all of the information necesssary to perform a request
// the coordinator will fill in this information from the websocket connection 
// and add it to the processing queue. Sleeping in this example is a placeholder
// for any long serial task.
struct request {
    server::handler::connection_ptr con;
    std::string req;
    std::string token;
    
    // test_message:uri=ws://localhost:9000;size=4096;count=1000;timeout=10000;binary=true;sync=true;correctness=exact;
    
    void process() {
        std::vector<wsperf::case_handler_ptr>  tests;
        std::string uri;
        
        wscmd::cmd command = wscmd::parse(req);
        
        if (command.command == "message_test") {
            uint64_t message_size;
            uint64_t message_count;
            uint64_t timeout;
            
            bool binary;
            bool sync;
            
            wsperf::correctness_mode mode;
            
            if (!extract_string(command,"uri",uri)) {return;}
            if (!extract_string(command,"token",token)) {return;}
            
            if (!extract_number(command,"size",message_size)) {return;}
            if (!extract_number(command,"count",message_count)) {return;}
            if (!extract_number(command,"timeout",timeout)) {return;}
            
            if (!extract_bool(command,"binary",binary)) {return;}
            if (!extract_bool(command,"sync",sync)) {return;}
            
            if (command.args["correctness"] == "exact") {
                mode = wsperf::EXACT;
            } else if (command.args["correctness"] == "length") {
                mode = wsperf::LENGTH;
            } else {
                send_error("Invalid correctness parameter");
                return;
            }
            
            tests.push_back(wsperf::case_handler_ptr(new wsperf::message_test(message_size,message_count,timeout,binary,sync,mode)));
        } else {
            send_error("Invalid Command");
            return;
        }
        
        std::stringstream response;
        response << "{\"type\":\"message\",\"data\":\"Starting test " << token << "\"}";
        con->send(response.str());
        
        // 9.1.x and 9.2.x tests
        /*for (int i = 1; i <= 2; i++) {
            for (int j = 1; j <= 6; j++) {
                tests.push_back(wsperf::case_handler_ptr(new wsperf::test_9_1_X(i,j)));
            }
        }*/
        
        // 9.7.x and 9.8.x tests
        /*for (int i = 7; i <= 8; i++) {
            for (int j = 1; j <= 6; j++) {
                tests.push_back(wsperf::case_handler_ptr(new wsperf::test_9_7_X(i,j)));
            }
        }*/
        
        
        
        client e(tests[0]);
        
        e.alog().unset_level(websocketpp::log::alevel::ALL);
        e.elog().unset_level(websocketpp::log::elevel::ALL);
        
        e.elog().set_level(websocketpp::log::elevel::ERROR);
        e.elog().set_level(websocketpp::log::elevel::FATAL);
        
        for (size_t i = 0; i < tests.size(); i++) {
            if (i > 0) {
                e.reset();
                e.set_handler(tests[i]);
            }
            
            e.connect(uri);
            e.run();
            
            std::stringstream json;
            
            /*json << "{\"type\":\"message\",\"data\":\"" << tests[i]->get_result() << "\"}";
            
            con->send(json.str());
            
            json.str("");*/
            
            json << "{\"type\":\"test_data\",\"target\":\"" << uri << "\",\"data\":" << tests[i]->get_data() << "}";
            
            con->send(json.str());
        }

        
        response.str("");
        response << "{\"type\":\"message\",\"data\":\"Test " << token << " complete.\"}";
        con->send(response.str());
    }
    
    bool extract_string(wscmd::cmd command,std::string key,std::string &val) {
        if (command.args[key] != "") {
           val = command.args[key];
           return true;
        } else {
            //std::stringstream response;
            //response << "{\"type\":\"message\",\"data\":\"" << key << " parameter is required.\"}";
            //con->send(response.str());
            con->send("{\"type\":\"message\",\"data\":\"Invalid " + key + " parameter.\"}");
            return false;
        }
    }
    
    bool extract_number(wscmd::cmd command,std::string key,uint64_t &val) {
        if (command.args[key] != "") {
            std::istringstream buf(command.args[key]);
            
            buf >> val;
            
            if (buf) {return true;}
        }
        //std::stringstream response;
        //response << "{\"type\":\"message\",\"data\":\"" << key << " parameter is required.\"}";
        //con->send(response.str());
        con->send("{\"type\":\"message\",\"data\":\"Invalid " + key + " parameter.\"}");
        return false;
    }
    
    bool extract_bool(wscmd::cmd command,std::string key,bool &val) {
        if (command.args[key] != "") {
            if (command.args[key] == "true") {
                val = true;
                return true;
            } else if (command.args[key] == "false") {
                val = false;
                return true;
            }
        }
        //std::stringstream response;
        //response << ;
        con->send("{\"type\":\"message\",\"data\":\"Invalid " + key + " parameter.\"}");
        return false;
    }
    
    void send_error(std::string msg) {
        std::stringstream response;
        response << "{\"type\":\"message\",\"data\":\"" << msg << "\"}";
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
        request r;
        r.con = con;
        r.req = msg->get_payload();
        m_coordinator.add_request(r);
    }
private:
    request_coordinator& m_coordinator;
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
            std::cout << "bad thread number" << std::endl;
            return 1;
        } else {
            h = server::handler::ptr(new concurrent_server_handler(rc));
        }
        
        server echo_endpoint(h);
        
        echo_endpoint.alog().unset_level(websocketpp::log::alevel::ALL);
        echo_endpoint.elog().unset_level(websocketpp::log::elevel::ALL);
        
        echo_endpoint.elog().set_level(websocketpp::log::elevel::ERROR);
        echo_endpoint.elog().set_level(websocketpp::log::elevel::FATAL);
        
        std::list<boost::shared_ptr<boost::thread> > threads;
        
        for (int i = 0; i < num_threads; i++) {
            threads.push_back(boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&process_requests, &rc))));
        }
        
        std::cout << "Starting wsperf server on port " << port << " with " << num_threads << " processing threads." << std::endl;
        echo_endpoint.listen(port);
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    
    return 0;
}
