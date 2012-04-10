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
 
#include "generic.hpp"
 
using wsperf::message_test;

// Construct a message_test from a wscmd command
/* Reads values from the wscmd object into member variables. The cmd object is
 * passed to the parent constructor for extracting values common to all test
 * cases.
 * 
 * Any of the constructors may throw a `case_exception` if required parameters
 * are not found or default values don't make sense.
 *
 * Values that message_test checks for:
 *
 * size=[interger];
 * Example: size=4096;
 * Size of messages to send in bytes. Valid values 0 - max size_t
 *
 * count=[integer];
 * Example: count=1000;
 * Number of test messages to send. Valid values 0-2^64
 * 
 * timeout=[integer];
 * Example: timeout=10000;
 * How long to wait (in ms) for a response before failing the test.
 * 
 * binary=[bool];
 * Example: binary=true;
 * Whether or not to use binary websocket frames. (true=binary, false=utf8)
 * 
 * sync=[bool];
 * Example: sync=true;
 * Syncronize messages. When sync is on wsperf will wait for a response before
 * sending the next message. When sync is off, messages will be sent as quickly
 * as possible.
 * 
 * correctness=[string];
 * Example: correctness=exact;
 * Example: correctness=length;
 * How to evaluate the correctness of responses. Exact checks each response for
 * exact correctness. Length checks only that the response has the correct 
 * length. Length mode is faster but won't catch invalid implimentations. Length
 * mode can be used to test situations where you deliberately return incorrect
 * bytes in order to compare performance (ex: performance with/without masking)
 */
message_test::message_test(wscmd::cmd& cmd) 
 : case_handler(cmd), 
   m_message_size(extract_number<uint64_t>(cmd,"size")),
   m_message_count(extract_number<uint64_t>(cmd,"count")),
   m_timeout(extract_number<uint64_t>(cmd,"timeout")),
   m_binary(extract_bool(cmd,"binary")),
   m_sync(extract_bool(cmd,"sync")),
   m_acks(0)
{
    if (cmd.args["correctness"] == "exact") {
        m_mode = EXACT;
    } else if (cmd.args["correctness"] == "length") {
        m_mode = LENGTH;
    } else {
        throw case_exception("Invalid correctness parameter.");
    }
}

void message_test::on_open(connection_ptr con) {
    con->alog()->at(websocketpp::log::alevel::DEVEL) 
        << "message_test::on_open" << websocketpp::log::endl;
    
    m_msg = con->get_data_message();
    
    m_data.reserve(static_cast<size_t>(m_message_size));
    
    if (!m_binary) {
        fill_utf8(m_data,static_cast<size_t>(m_message_size),true);
        m_msg->reset(websocketpp::frame::opcode::TEXT);
    } else {
        fill_binary(m_data,static_cast<size_t>(m_message_size),true);
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

void message_test::on_message(connection_ptr con,websocketpp::message::data_ptr msg) {
    if ((m_mode == LENGTH && msg->get_payload().size() == m_data.size()) || 
        (m_mode == EXACT && msg->get_payload() == m_data))
    {
        m_acks++;
        m_bytes += m_message_size;
        mark();
    } else {
        mark();
        m_msg.reset();
        m_pass = FAIL;
        
        this->end(con);
    }
    
    if (m_acks == m_message_count) {
        m_pass = PASS;
        m_msg.reset();
        this->end(con);
    } else if (m_sync && m_pass == RUNNING) {
        con->send(m_msg);
    }
}
