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

enum request_type {
    PERF_TEST = 0,
    END_WORKER = 1
};

class writer {
public:
	virtual ~writer() {}
	
    virtual void write(std::string msg) = 0;
};

typedef boost::shared_ptr<writer> writer_ptr;

template <typename endpoint_type>
class ws_writer : public writer {
public:
    ws_writer(typename endpoint_type::handler::connection_ptr con) 
     : m_con(con) {}
    
    void write(std::string msg) {
        m_con->send(msg);
    }
private:
    typename endpoint_type::handler::connection_ptr m_con;
};

// A request encapsulates all of the information necesssary to perform a request
// the coordinator will fill in this information from the websocket connection 
// and add it to the processing queue. Sleeping in this example is a placeholder
// for any long serial task.
struct request {
    writer_ptr  writer;
    
    request_type type;
    std::string req;                // The raw request
    std::string token;              // Parsed test token. Return in all results    
    
    /// Run a test and return JSON result
    void process(unsigned int id);
    
    // Simple json generation
    std::string prepare_response(std::string type,std::string data);
    std::string prepare_response_object(std::string type,std::string data);
};

// The coordinator is a simple wrapper around an STL queue. add_request inserts
// a new request. get_request returns the next available request and blocks
// (using condition variables) in the case that the queue is empty.
class request_coordinator {
public:
    void add_request(const request& r) {
        {
            boost::unique_lock<boost::mutex> lock(m_lock);
            m_requests.push(r);
        }
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
    
    void reset() {
        boost::unique_lock<boost::mutex> lock(m_lock);
        
        while (!m_requests.empty()) {
            m_requests.pop();
        }
    }
private:
    std::queue<request>         m_requests;
    boost::mutex                m_lock;
    boost::condition_variable   m_cond;
};

/// Handler that reads requests off the wire and dispatches them to a request queue
template <typename endpoint_type>
class concurrent_handler : public endpoint_type::handler {
public:
    typedef typename endpoint_type::handler::connection_ptr connection_ptr;
    typedef typename endpoint_type::handler::message_ptr message_ptr;
    
    concurrent_handler(request_coordinator& c,
                       std::string ident, 
                       std::string ua,
                       unsigned int num_workers)
     : m_coordinator(c), 
       m_ident(ident), 
       m_ua(ua),
       m_num_workers(num_workers),
       m_blocking(num_workers == 0) {}
    
    void on_open(connection_ptr con) {
        std::stringstream o;
        
        o << "{"
          << "\"type\":\"test_welcome\","
          << "\"version\":\"" << m_ua << "\","
          << "\"ident\":\"" << m_ident << "\","
          << "\"num_workers\":" << m_num_workers 
          << "}";
        
        con->send(o.str());
    }
    
    void on_message(connection_ptr con, message_ptr msg) {
        request r;
        r.type = PERF_TEST;
        r.writer = writer_ptr(new ws_writer<endpoint_type>(con));
        r.req = msg->get_payload();
        
        if (m_blocking) {
            r.process(0);
        } else {
            m_coordinator.add_request(r);
        }
    }
    
    void on_fail(connection_ptr con) {
        std::cout << "A command connection failed." << std::endl;
    }
    
    void on_close(connection_ptr con) {
        std::cout << "A command connection closed." << std::endl;
    }
private:
    request_coordinator&    m_coordinator;
    std::string             m_ident;
    std::string             m_ua;
    unsigned int            m_num_workers;
    bool                    m_blocking;
};

// process_requests is the body function for a processing thread. It loops 
// forever reading requests, processing them serially, then reading another 
// request. A request with type END_WORKER will stop the processing loop.
void process_requests(request_coordinator* coordinator, unsigned int id);

} // namespace wsperf

#endif // WSPERF_REQUEST_HPP
