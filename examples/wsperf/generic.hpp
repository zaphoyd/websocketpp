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

class message_test : public case_handler {
public: 
    /// Construct a message test from a wscmd command
    explicit message_test(wscmd::cmd& cmd);
    
    void on_open(connection_ptr con);
    void on_message(connection_ptr con,websocketpp::message::data_ptr msg);
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
