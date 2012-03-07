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

#ifndef WSPERF_REQUEST_HPP
#define WSPERF_REQUEST_HPP

#include "case.hpp"
#include "generic.hpp"
#include "wscmd.hpp"

#include "../../src/roles/client.hpp"
#include "../../src/websocketpp.hpp"

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

using websocketpp::client;
using websocketpp::server;

namespace wsperf {

class writer {
public:
    virtual void write(std::string msg) = 0;
};

typedef boost::shared_ptr<writer> writer_ptr;

template <typename T>
class ws_writer : public writer {
public:
    ws_writer(typename T::handler::connection_ptr con) : m_con(con) {}
    
    void write(std::string msg) {
        m_con->send(msg);
    }
private:
    typename T::handler::connection_ptr m_con;
};

// A request encapsulates all of the information necesssary to perform a request
// the coordinator will fill in this information from the websocket connection 
// and add it to the processing queue. Sleeping in this example is a placeholder
// for any long serial task.
struct request {
    writer_ptr  writer;
    
    std::string req;                // The raw request
    std::string token;              // Parsed test token. Return in all results    
    
    /// Run a test and return JSON result
    void process() {
        case_handler_ptr test;
        std::string uri;
        
        wscmd::cmd command = wscmd::parse(req);
        
        try {
            if (command.command == "message_test") {
                test = case_handler_ptr(new message_test(command));
                token = test->get_token();
            } else {
                writer->write(prepare_response("error","Invalid Command"));
                return;
            }
        } catch (case_exception& e) {
            writer->write(prepare_response("error",e.what()));
            return;
        }
        
        writer->write(prepare_response("test_start",""));
        
        client e(test);
        
        e.alog().unset_level(websocketpp::log::alevel::ALL);
        e.elog().unset_level(websocketpp::log::elevel::ALL);
        
        e.elog().set_level(websocketpp::log::elevel::RERROR);
        e.elog().set_level(websocketpp::log::elevel::FATAL);
        
        e.connect(uri);
        e.run();
        
        writer->write(prepare_response("test_start",test->get_data()));

        writer->write(prepare_response("test_complete",""));
    }
    
    std::string prepare_response(std::string type,std::string data) {
        return "{\"type\":\"" + type + "\",\"token\":\"" + token + "\",\"data\":\"" + data + "\"}";
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
        r.writer = writer_ptr(new ws_writer<server>(con));
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

} // namespace wsperf








#endif // WSPERF_REQUEST_HPP