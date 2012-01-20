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


#include "../../src/sockets/tls.hpp"
#include "../../src/websocketpp.hpp"

#include "broadcast_server_handler.hpp"

#include <boost/date_time/posix_time/posix_time.hpp>

#include <cstring>
#include <set>

#include <sys/resource.h>

//typedef websocketpp::endpoint<websocketpp::role::server,websocketpp::socket::plain> plain_endpoint_type;
//typedef plain_endpoint_type::handler_ptr plain_handler_ptr;

//typedef websocketpp::endpoint<websocketpp::role::server,websocketpp::socket::ssl> tls_endpoint_type;
//typedef tls_endpoint_type::handler_ptr tls_handler_ptr;

using websocketpp::server;
using websocketpp::server_tls;

int main(int argc, char* argv[]) {
    unsigned short port = 9002;
    bool tls = false;
    
    // 12288 is max OS X limit without changing kernal settings
    const rlim_t ideal_size = 10000;
    rlim_t old_size;
    rlim_t old_max;
    
    struct rlimit rl;
    int result;
    
    result = getrlimit(RLIMIT_NOFILE, &rl);
    if (result == 0) {
        //std::cout << "System FD limits: " << rl.rlim_cur << " max: " << rl.rlim_max << std::endl;
        
        old_size = rl.rlim_cur;
        old_max = rl.rlim_max;
        
        if (rl.rlim_cur < ideal_size) {
            std::cout << "Attempting to raise system file descriptor limit from " << rl.rlim_cur << " to " << ideal_size << std::endl;
            rl.rlim_cur = ideal_size;
            
            if (rl.rlim_max < ideal_size) {
                rl.rlim_max = ideal_size;
            }
            
            result = setrlimit(RLIMIT_NOFILE, &rl);
            
            if (result == 0) {
                std::cout << "Success" << std::endl;
            } else if (result == EPERM) {
                std::cout << "Failed. This server will be limited to " << old_size << " concurrent connections. Error code: Insufficient permissions. Try running process as root. system max: " << old_max << std::endl;
            } else {
                std::cout << "Failed. This server will be limited to " << old_size << " concurrent connections. Error code: " << errno << " system max: " << old_max << std::endl;
            }
        }
    }
    
    if (argc == 2) {
        // TODO: input validation?
        port = atoi(argv[1]);
    }
    
    if (argc == 3) {
        // TODO: input validation?
        port = atoi(argv[1]);
        tls = !strcmp(argv[2],"-tls");
    }
    
    try {
        if (tls) {
            server_tls::handler_ptr handler(new websocketpp::broadcast::server_handler<server_tls>());
            server_tls endpoint(handler);
            
            endpoint.alog().unset_level(websocketpp::log::alevel::ALL);
            endpoint.elog().set_level(websocketpp::log::elevel::ALL);
            
            std::cout << "Starting Secure WebSocket broadcast server on port " << port << std::endl;
            endpoint.listen(port);
        } else {
            server::handler_ptr handler(new websocketpp::broadcast::server_handler<server>());
            server endpoint(handler);
            
            endpoint.alog().unset_level(websocketpp::log::alevel::ALL);
            endpoint.elog().set_level(websocketpp::log::elevel::ALL);
            
            //endpoint.alog().set_level(websocketpp::log::alevel::DEVEL);
            
            std::cout << "Starting WebSocket broadcast server on port " << port << std::endl;
            endpoint.listen(port);
        }
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        
    }
    
    return 0;
}
