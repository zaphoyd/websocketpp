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

using wsperf::request;

void request::process() {
    case_handler_ptr test;
    std::string uri;
    
    wscmd::cmd command = wscmd::parse(req);
    
    try {
        if (command.command == "message_test") {
            test = case_handler_ptr(new message_test(command));
            token = test->get_token();
            uri = test->get_uri();
        } else {
            writer->write(prepare_response("error","Invalid Command"));
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
        
        writer->write(prepare_response_object("test_data",test->get_data()));

        writer->write(prepare_response("test_complete",""));
    } catch (case_exception& e) {
        writer->write(prepare_response("error",e.what()));
        return;
    } catch (websocketpp::exception& e) {
        writer->write(prepare_response("error",e.what()));
        return;
    } catch (websocketpp::uri_exception& e) {
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

void wsperf::process_requests(request_coordinator* coordinator) {
    request r;
    
    while (1) {
        coordinator->get_request(r);
        
        if (r.type == PERF_TEST) {
            r.process();
        } else {
            break;
        }
    }
}