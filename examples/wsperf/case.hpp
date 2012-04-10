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

#ifndef WSPERF_CASE_HPP
#define WSPERF_CASE_HPP

#include "wscmd.hpp"

#include "../../src/roles/client.hpp"
#include "../../src/websocketpp.hpp"

#include <boost/chrono.hpp>

#include <exception>

using websocketpp::client;

namespace wsperf {

class case_exception : public std::exception {
public: 
    case_exception(const std::string& msg) : m_msg(msg) {}
    ~case_exception() throw() {}
    
    virtual const char* what() const throw() {
        return m_msg.c_str();
    }
    
    std::string m_msg;
};

class case_handler : public client::handler {
public:
    typedef case_handler type;
    
    /// Construct a message test from a wscmd command
    explicit case_handler(wscmd::cmd& cmd);
    
    void start(connection_ptr con, uint64_t timeout);
    void end(connection_ptr con);
    
    void mark();
    
    // Just does random ascii right now. True random UTF8 with multi-byte stuff
    // would probably be better
    void fill_utf8(std::string& data,size_t size,bool random = true);
    void fill_binary(std::string& data,size_t size,bool random = true);
    
    void on_timer(connection_ptr con,const boost::system::error_code& error);
    
    void on_close(connection_ptr con) {
        con->alog()->at(websocketpp::log::alevel::DEVEL) 
            << "case_handler::on_close" 
            << websocketpp::log::endl;
    }
    void on_fail(connection_ptr con);
    
    const std::string& get_data() const;
    const std::string& get_token() const;
    const std::string& get_uri() const;
    
    // TODO: refactor these three extract methods into wscmd
    std::string extract_string(wscmd::cmd command,std::string key) {
        if (command.args[key] != "") {
           return command.args[key];
        } else {
            throw case_exception("Invalid " + key + " parameter.");
        }
    }
    
    template <typename T>
    T extract_number(wscmd::cmd command,std::string key) {
        if (command.args[key] != "") {
            std::istringstream buf(command.args[key]);
            T val;
            
            buf >> val;
            
            if (buf) {return val;}
        }
        throw case_exception("Invalid " + key + " parameter.");
    }
    
    bool extract_bool(wscmd::cmd command,std::string key) {
        if (command.args[key] != "") {
            if (command.args[key] == "true") {
                return true;
            } else if (command.args[key] == "false") {
                return false;
            }
        }
        throw case_exception("Invalid " + key + " parameter.");
    }
protected:
    enum status {
        FAIL = 0,
        PASS = 1,
        TIME_OUT = 2,
        RUNNING = 3
    };
    
    std::string                 m_uri;
    std::string                 m_token;
    size_t                      m_quantile_count;
    bool                        m_rtts;
    std::string                 m_data;
    
    status                      m_pass;
    
    uint64_t                                                m_timeout;
    boost::shared_ptr<boost::asio::deadline_timer>          m_timer;
    
    boost::chrono::steady_clock::time_point                 m_start;
    std::vector<boost::chrono::steady_clock::time_point>    m_end;
    std::vector<double>                                     m_times;
    
    uint64_t                    m_bytes;
};

typedef boost::shared_ptr<case_handler> case_handler_ptr;

} // namespace wsperf

#endif // WSPERF_CASE_HPP
