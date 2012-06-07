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

#include "../../src/roles/client.hpp"
#include "../../src/websocketpp.hpp"

#include <boost/program_options.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

#include <cstring>
#include <sstream>
#include <fstream>

// This default will only work on unix systems.
// Windows systems should set this as a compile flag to an appropriate value
#ifndef WSPERF_CONFIG
    #define WSPERF_CONFIG "~/.wsperf"
#endif

static const std::string user_agent = "wsperf/0.2.0dev "+websocketpp::USER_AGENT;

using websocketpp::client;
namespace po = boost::program_options;

int start_server(po::variables_map& vm);
int start_client(po::variables_map& vm);

int start_server(po::variables_map& vm) {
    unsigned short port = vm["port"].as<unsigned short>();
    unsigned int num_threads = vm["num_threads"].as<unsigned int>();
    std::string ident = vm["ident"].as<std::string>();
    
    bool silent = (vm.count("silent") && vm["silent"].as<int>() == 1);
    
    std::list< boost::shared_ptr<boost::thread> > threads;
    
    wsperf::request_coordinator rc;
    
    server::handler::ptr h;
    h = server::handler::ptr(
        new wsperf::concurrent_handler<server>(
            rc,
            ident,
            user_agent,
            num_threads
        )
    );
    
    if (!silent) {
        std::cout << "Starting wsperf server on port " << port << " with " << num_threads << " processing threads." << std::endl;
    }
    
    // Start worker threads
    for (unsigned int i = 0; i < num_threads; i++) {
        threads.push_back(boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&wsperf::process_requests, &rc, i))));
    }
    
    // Start WebSocket++
    server endpoint(h);
    
    endpoint.alog().unset_level(websocketpp::log::alevel::ALL);
    endpoint.elog().unset_level(websocketpp::log::elevel::ALL);
    
    if (!silent) {
        endpoint.alog().set_level(websocketpp::log::alevel::CONNECT);
        endpoint.alog().set_level(websocketpp::log::alevel::DISCONNECT);
        
        endpoint.elog().set_level(websocketpp::log::elevel::RERROR);
        endpoint.elog().set_level(websocketpp::log::elevel::FATAL);
    }
    
    endpoint.listen(port);
    
    return 0;
}

int start_client(po::variables_map& vm) {
    if (!vm.count("uri")) {
        std::cerr << "client mode requires uri" << std::endl;
        return 1;
    }
    
    bool silent = (vm.count("silent") && vm["silent"].as<int>() == 1);
    
    unsigned int reconnect = vm["reconnect"].as<unsigned int>();
    
    std::string uri = vm["uri"].as<std::string>();
    unsigned int num_threads = vm["num_threads"].as<unsigned int>();
    std::string ident = vm["ident"].as<std::string>();
    
    // Start wsperf
    std::list< boost::shared_ptr<boost::thread> > threads;
    std::list< boost::shared_ptr<boost::thread> >::iterator thread_it;
    
    wsperf::request_coordinator rc;
    
    client::handler::ptr h;
    h = client::handler::ptr(
        new wsperf::concurrent_handler<client>(
            rc,
            ident,
            user_agent,
            num_threads
        )
    );
    
    if (!silent) {
        std::cout << "Starting wsperf client connecting to " << uri << " with " << num_threads << " processing threads." << std::endl;
    }
    
    for (unsigned int i = 0; i < num_threads; i++) {
        threads.push_back(boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&wsperf::process_requests, &rc, i))));
    }
    
    while(1) {
        client endpoint(h);
        
        endpoint.alog().unset_level(websocketpp::log::alevel::ALL);
        endpoint.elog().unset_level(websocketpp::log::elevel::ALL);
        
        if (!silent) {
            endpoint.alog().set_level(websocketpp::log::alevel::CONNECT);
            endpoint.alog().set_level(websocketpp::log::alevel::DISCONNECT);
            
            endpoint.elog().set_level(websocketpp::log::elevel::RERROR);
            endpoint.elog().set_level(websocketpp::log::elevel::FATAL);
        }
        
        client::connection_ptr con = endpoint.get_connection(uri);
        
        con->add_request_header("User-Agent",user_agent);
        con->add_subprotocol("wsperf");
        
        endpoint.connect(con);
        
        // This will block until there is an error or the websocket closes
        endpoint.run();
        
        rc.reset();
        
        if (!reconnect) {
            break;
        } else {
            boost::this_thread::sleep(boost::posix_time::seconds(reconnect));
        }
    }
    
    // Add a "stop work" request for each outstanding worker thread
    for (thread_it = threads.begin(); thread_it != threads.end(); ++thread_it) {
        wsperf::request r;
        r.type = wsperf::END_WORKER;
        rc.add_request(r);
    }
    
    // Wait for worker threads to finish quitting.
    for (thread_it = threads.begin(); thread_it != threads.end(); ++thread_it) {
        (*thread_it)->join();
    }
        
    return 0;
}

int main(int argc, char* argv[]) {
    try {
        // 12288 is max OS X limit without changing kernal settings
        /*const rlim_t ideal_size = 10000;
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
        }*/
        
        std::string config_file;
        
        // Read and Process Command Line Options
        po::options_description generic("Generic");
        generic.add_options()
            ("help", "produce this help message")
            ("version,v", po::value<int>()->implicit_value(1), "Print version information")
            ("config", po::value<std::string>(&config_file)->default_value(WSPERF_CONFIG),
                  "Configuration file to use.")
        ;
        
        po::options_description config("Configuration");
        config.add_options()
            ("server,s", po::value<int>()->implicit_value(1), "Run in server mode")
            ("client,c", po::value<int>()->implicit_value(1), "Run in client mode")
            ("port,p", po::value<unsigned short>()->default_value(9050), "Port to listen on in server mode")
            ("uri,u", po::value<std::string>(), "URI to connect to in client mode")
            ("reconnect,r", po::value<unsigned int>()->default_value(0), "Auto-reconnect delay (in seconds) after a connection ends or fails in client mode. Zero indicates do not reconnect.")
            ("num_threads", po::value<unsigned int>()->default_value(2), "Number of worker threads to use")
            ("silent", po::value<int>()->implicit_value(1), "Silent mode. Will not print errors to stdout")
            ("ident,i", po::value<std::string>()->default_value("Unspecified"), "Implimentation identification string reported by this agent.")
        ;
        
        po::options_description cmdline_options;
        cmdline_options.add(generic).add(config);
        
        po::options_description config_file_options;
        config_file_options.add(config);
        
        po::options_description visible("Allowed options");
        visible.add(generic).add(config);
        
        
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
        po::notify(vm);
        
        std::ifstream ifs(config_file.c_str());
        if (ifs) {
            store(parse_config_file(ifs, config_file_options), vm);
            notify(vm);
        }
        
        if (vm.count("help")) {
            std::cout << cmdline_options << std::endl;
            return 1;
        }
        
        if (vm.count("version")) {
            std::cout << user_agent << std::endl;
            return 1;
        }
        
        if (vm.count("server") && vm["server"].as<int>() == 1) {
            return start_server(vm);
        } else if (vm.count("client") && vm["client"].as<int>() == 1) {
            return start_client(vm);
        } else {
            std::cerr << "You must choose either client or server mode. See wsperf --help for more information" << std::endl;
            return 1;
        }
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    
    return 0;
}
