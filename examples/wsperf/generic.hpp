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

#ifndef WSPERF_CASE_GENERIC_HPP
#define WSPERF_CASE_GENERIC_HPP

#include "case.hpp"
#include "wscmd.hpp"

namespace wsperf {

enum correctness_mode {
    EXACT = 0,
    LENGTH = 1
};

// test class for 9.1.* and 9.2.*
class message_test : public case_handler {
public: 
    message_test(uint64_t message_size,
                 uint64_t message_count,
                 int timeout,
                 bool binary,
                 bool sync,
                 correctness_mode mode) 
     : m_message_size(message_size),
       m_message_count(message_count),
       m_timeout(timeout),
       m_binary(binary),
       m_sync(sync),
       m_mode(mode),
       m_acks(0)
    {}
    
    explicit message_test(wscmd::cmd& cmd) {
        // message_test:uri=[string];           ws://localhost:9000
        //              size=[interger];        4096
        //              count=[integer];        1000
        //              timeout=[integer];      10000
        //              binary=[bool];          true/false
        //              sync=[bool];            true/false
        //              correctness=[string];   exact/length
        //              token=[string];         foo
        
        m_uri = extract_string(cmd,"uri");
        m_token = extract_string(cmd,"token");
        
        m_message_count = extract_number<uint64_t>(cmd,"size");
        m_message_size = extract_number<uint64_t>(cmd,"count");
        m_timeout = extract_number<uint64_t>(cmd,"timeout");
        
        m_binary = extract_bool(cmd,"binary");
        m_sync = extract_bool(cmd,"sync");
        
        if (cmd.args["correctness"] == "exact") {
            m_mode = EXACT;
        } else if (cmd.args["correctness"] == "length") {
            m_mode = LENGTH;
        } else {
            throw case_exception("Invalid correctness parameter.");
        }
    }
    
    void on_open(connection_ptr con) {
        m_msg = con->get_data_message();
        
        m_data.reserve(m_message_size);
        
        if (!m_binary) {
            fill_utf8(m_data,m_message_size,true);
            m_msg->reset(websocketpp::frame::opcode::TEXT);
        } else {
            fill_binary(m_data,m_message_size,true);
            m_msg->reset(websocketpp::frame::opcode::BINARY);
        }
        
        m_msg->set_payload(m_data);
        
        start(con,m_timeout);
        
        if (m_sync) {
            con->send(m_msg);
        } else {
            for (uint64_t i = 0; i < m_message_count; i++) {
                con->send(m_msg);
            }
        }
    }
    
    void on_message(connection_ptr con,websocketpp::message::data_ptr msg) {
        if ((m_mode == LENGTH && msg->get_payload().size() == m_data.size()) || (m_mode == EXACT && msg->get_payload() == m_data)) {
            m_acks++;
            m_bytes += m_message_size;
            mark();
        } else {
            mark();
            m_timer->cancel();
            m_msg.reset();
            m_pass = FAIL;
            std::cout << "foo" << std::endl;
            this->end(con);
        }
        
        if (m_acks == m_message_count) {
            m_pass = PASS;
            m_timer->cancel();
            m_msg.reset();
            this->end(con);
        } else if (m_sync && m_pass == RUNNING) {
            con->send(m_msg);
        }
    }
private:
    // Simulation Parameters
    uint64_t            m_message_size;
    uint64_t            m_message_count;
    uint64_t            m_timeout;
    bool                m_binary;
    bool                m_sync;
    correctness_mode    m_mode;
    
    // Simulation temporaries
    std::string         m_data;
    message_ptr         m_msg;
    uint64_t            m_acks;
};

} // namespace wsperf

#endif // WSPERF_CASE_GENERIC_HPP