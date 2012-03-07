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

#include "request.hpp"

#include "../../src/websocketpp.hpp"

#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

#include <cstring>
#include <sstream>

using websocketpp::server;

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
    unsigned short port = 9003;
    unsigned short num_threads = 2;
    
    try {
        std::list< boost::shared_ptr<boost::thread> > threads;
        
        if (argc == 2) {
            std::stringstream buffer(argv[1]);
            buffer >> port;
        }
        
        if (argc == 3) {
            std::stringstream buffer(argv[2]);
            buffer >> num_threads;
        }
        
        wsperf::request_coordinator rc;
        
        server::handler::ptr h;
        if (num_threads == 0) {
            std::cout << "bad thread number" << std::endl;
            return 1;
        } else {
            h = server::handler::ptr(new wsperf::concurrent_server_handler(rc));
        }
        
        server echo_endpoint(h);
        
        echo_endpoint.alog().unset_level(websocketpp::log::alevel::ALL);
        echo_endpoint.elog().unset_level(websocketpp::log::elevel::ALL);
        
        echo_endpoint.elog().set_level(websocketpp::log::elevel::RERROR);
        echo_endpoint.elog().set_level(websocketpp::log::elevel::FATAL);
        
        for (int i = 0; i < num_threads; i++) {
            threads.push_back(boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&wsperf::process_requests, &rc))));
        }
        
        std::cout << "Starting wsperf server on port " << port << " with " << num_threads << " processing threads." << std::endl;
        echo_endpoint.listen(port);
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    
    return 0;
}
