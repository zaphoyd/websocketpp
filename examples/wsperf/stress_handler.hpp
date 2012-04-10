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

#ifndef WSPERF_STRESS_HANDLER_HPP
#define WSPERF_STRESS_HANDLER_HPP

#include "wscmd.hpp"

#include "../../src/roles/client.hpp"
#include "../../src/websocketpp.hpp"

#include <boost/chrono.hpp>
#include <boost/thread/mutex.hpp>

#include <exception>

using websocketpp::client;

namespace wsperf {

class stress_handler : public client::handler {
public:
    typedef stress_handler type;
    
    /// Construct a stress test from a wscmd command
    explicit stress_handler(wscmd::cmd& cmd);
    
    void on_open(connection_ptr con);
    void on_close(connection_ptr con);
    void on_fail(connection_ptr con);
    
    void start(connection_ptr con);
    void end();
    
    std::string get_data() const;
protected:
    size_t m_current_connections;
    size_t m_max_connections;
    size_t m_total_connections;
    size_t m_failed_connections;
        
    // Stats update timer
    size_t m_timeout;
    boost::shared_ptr<boost::asio::deadline_timer> m_timer;
    
    mutable boost::mutex m_lock;
};

typedef boost::shared_ptr<stress_handler> stress_handler_ptr;

} // namespace wsperf

#endif // WSPERF_STRESS_HANDLER_HPP
