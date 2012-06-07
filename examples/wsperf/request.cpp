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
#include "stress_aggregate.hpp"

#include <algorithm>
#include <sstream>

using wsperf::request;

void request::process(unsigned int id) {
    case_handler_ptr test;
    stress_handler_ptr shandler;
    std::string uri;
    size_t connections_opened = 0;
    size_t connection_count;
    
    wscmd::cmd command = wscmd::parse(req);
    
    try {
        if (command.command == "message_test") {
            test = case_handler_ptr(new message_test(command));
            token = test->get_token();
            uri = test->get_uri();
        } else if (command.command == "stress_test") {
            shandler = stress_handler_ptr(new stress_aggregate(command));
            
            // todo make sure this isn't 0
            if(!wscmd::extract_number<size_t>(command, "connection_count",connection_count)) {
                connection_count = 1;
            }
            
            if (command.args["token"] != "") {
                token = command.args["token"];
            } else {
                throw case_exception("Invalid token parameter.");
            }
            
            if (command.args["uri"] != "") {
                uri = command.args["uri"];
            } else {
                throw case_exception("Invalid uri parameter.");
            }
        } else {
            writer->write(prepare_response("error","Invalid Command"));
            return;
        }
        
        std::stringstream o;
        o << "{\"worker_id\":" << id << "}";
        
        writer->write(prepare_response_object("test_start",o.str()));
        
        if (command.command == "message_test") {
            client e(test);
            
            e.alog().set_level(websocketpp::log::alevel::ALL);
            e.elog().set_level(websocketpp::log::elevel::ALL);
            
            //e.alog().unset_level(websocketpp::log::alevel::ALL);
            //e.elog().unset_level(websocketpp::log::elevel::ALL);
            
            //e.elog().set_level(websocketpp::log::elevel::RERROR);
            //e.elog().set_level(websocketpp::log::elevel::FATAL);
                        
            e.connect(uri);
            e.run();
            
            writer->write(prepare_response_object("test_data",test->get_data()));
        } else if (command.command == "stress_test") {
            client e(shandler);
            
            e.alog().unset_level(websocketpp::log::alevel::ALL);
            e.elog().unset_level(websocketpp::log::elevel::ALL);
            
            /*client::connection_ptr con;
            con = e.get_connection(uri);
            shandler->on_connect(con);
            e.connect(con);*/
            
            boost::thread t(boost::bind(&client::run, &e, true));
            
            size_t handshake_delay;
            if(!wscmd::extract_number<size_t>(command, "handshake_delay",handshake_delay)) {
                handshake_delay = 10;
            }
            
            // open n connections
            for (size_t i = 0; i < connection_count; i++) {
                client::connection_ptr con;
                con = e.get_connection(uri);
                shandler->on_connect(con);
                e.connect(con);
            
                boost::this_thread::sleep(boost::posix_time::milliseconds(handshake_delay));
            }
            
            // start sending messages 
            shandler->start_message_test();
            
            e.end_perpetual();
            
            t.join();
            
            std::cout << "writing data" << std::endl;
            writer->write(prepare_response_object("test_data",shandler->get_data()));
            
            /*for (;;) {
                // tell the handler to perform its event loop
                
                bool quit = false;
                
                if (connections_opened == connection_count) {
                    std::cout << "maintenance loop" << std::endl;
                    quit = shandler->maintenance();
                }
                
                // check for done-ness
                if (quit) {
                    // send update to command
                    std::cout << "writing data" << std::endl;
                    writer->write(prepare_response_object("test_data",shandler->get_data()));
                    break;
                }
                
                // unless we know we have something to do, sleep for a bit.
                if (connections_opened < connection_count) {
                    continue;
                }
                
                boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
            }*/
            //e.end_perpetual();
            
        }

        writer->write(prepare_response("test_complete",""));
    } catch (case_exception& e) {
        std::cout << "case_exception: " << e.what() << std::endl;
        writer->write(prepare_response("error",e.what()));
        return;
    } catch (websocketpp::exception& e) {
        std::cout << "websocketpp::exception: " << e.what() << std::endl;
        writer->write(prepare_response("error",e.what()));
        return;
    } catch (websocketpp::uri_exception& e) {
        std::cout << "websocketpp::uri_exception: " << e.what() << std::endl;
        writer->write(prepare_response("error",e.what()));
        return;
    }
}

std::string request::prepare_response(std::string type,std::string data) {
    return "{\"type\":\"" + type 
            + "\",\"token\":\"" + token + "\",\"data\":\"" + data + "\"}";
}
    
std::string request::prepare_response_object(std::string type,std::string data){
    return "{\"type\":\"" + type 
            + "\",\"token\":\"" + token + "\",\"data\":" + data + "}";
}

void wsperf::process_requests(request_coordinator* coordinator,unsigned int id) {
    request r;
    
    while (1) {
        coordinator->get_request(r);
        
        if (r.type == PERF_TEST) {
            r.process(id);
        } else {
            break;
        }
    }
}