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

#include "stress_handler.hpp"

using wsperf::stress_handler;

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
 * uri=[string];
 * Example: uri=ws://localhost:9000;
 * URI of the server to connect to
 * 
 * token=[string];
 * Example: token=foo;
 * String value that will be returned in the `token` field of all test related
 * messages. A separate token should be sent for each unique test.
 *
 * quantile_count=[integer];
 * Example: quantile_count=10;
 * How many histogram quantiles to return in the test results
 * 
 * rtts=[bool];
 * Example: rtts:true;
 * Whether or not to return the full list of round trip times for each message
 * primarily useful for debugging.
 */
stress_handler::stress_handler(wscmd::cmd& cmd)
  : m_current_connections(0)
  , m_max_connections(0)
  , m_total_connections(0)
  , m_failed_connections(0)
  , m_next_con_id(0)
  , m_init(boost::chrono::steady_clock::now())
  , m_next_msg_id(0)
  , m_con_sync(false)
{
    if (!wscmd::extract_number<size_t>(cmd,"msg_count",m_msg_count)) {
        m_msg_count = 0;
    }
    
    if (!wscmd::extract_number<size_t>(cmd,"msg_size",m_msg_size)) {
        m_msg_size = 0;
    }
    
    std::string mode;
    if (wscmd::extract_string(cmd,"msg_mode",mode)) {
        if (mode == "fixed") {
            m_msg_mode = msg_mode::FIXED;
        } else if (mode == "infinite") {
            m_msg_mode = msg_mode::UNLIMITED;
        } else {
            m_msg_mode = msg_mode::NONE;
        }
    } else {
         m_msg_mode = msg_mode::NONE;
    }
    
    if (wscmd::extract_string(cmd,"con_lifetime",mode)) {
        if (mode == "random") {
            m_con_lifetime = con_lifetime::RANDOM;
        } else if (mode == "infinite") {
            m_con_lifetime = con_lifetime::UNLIMITED;
        } else {
            m_con_lifetime = con_lifetime::FIXED;
        }
    } else {
        m_con_lifetime = con_lifetime::FIXED;
    }
    
    if (m_con_lifetime == con_lifetime::FIXED) {
        if (!wscmd::extract_number<size_t>(cmd,"con_duration",m_con_duration)) {
            m_con_duration = 5000;
        }
    } else if (m_con_lifetime == con_lifetime::RANDOM) {
        size_t max_dur;
        if (!wscmd::extract_number<size_t>(cmd,"con_duration",max_dur)) {
            max_dur = 5000;
        }
        
        // TODO: choose random number between 0 and max_dur
        m_con_duration = max_dur;
    }
}

void stress_handler::on_connect(connection_ptr con) {
    boost::lock_guard<boost::mutex> lock(m_lock);
    
    m_con_data[con] = con_data(m_next_con_id++, m_init);
    m_con_data[con].start = boost::chrono::steady_clock::now();
}

void stress_handler::on_handshake_init(connection_ptr con) {
    boost::lock_guard<boost::mutex> lock(m_lock);
        
    m_con_data[con].tcp_established = boost::chrono::steady_clock::now();
    
    // TODO: log close reason?
}

void stress_handler::start_message_test() {
    m_msg.reset(new std::string());
    m_msg->assign(m_msg_size,'*');
    
    // for each connection send the first message
    std::map<connection_ptr,con_data>::iterator it;
    for (it = m_con_data.begin(); it != m_con_data.end(); it++) {
        connection_ptr con = (*it).first;
        con_data& data = (*it).second;
        
        /*data.msg = con->get_data_message();
        std::string msg;
        msg.assign(m_msg_size,'*');
        
        data.msg->set_payload(msg);
        
        data.msg->reset(websocketpp::frame::opcode::BINARY);*/
        
        boost::lock_guard<boost::mutex> lock(m_lock);
        
        msg_data foo;
        
        foo.msg_id = m_next_msg_id++;        
        foo.send_time = boost::chrono::steady_clock::now();
        data.messages.push_back(foo);
        
        con->send(*m_msg);
    }
}

void stress_handler::on_message(connection_ptr con,websocketpp::message::data_ptr msg) {
    time_point mark = boost::chrono::steady_clock::now();
    
    boost::lock_guard<boost::mutex> lock(m_lock);
    
    std::map<connection_ptr,con_data>::iterator element = m_con_data.find(con);
    
    if (element == m_con_data.end()) {
        std::cout << "Bad Bad Bad" << std::endl;
        return;
    }
    
    con_data& data = (*element).second;
    
    data.messages.back().recv_time = mark;
    
        
    if (data.messages.size() < m_msg_count) {
        msg_data foo;
        
        foo.msg_id = m_next_msg_id++;        
        foo.send_time = boost::chrono::steady_clock::now();
        data.messages.push_back(foo);
        
        con->send(*m_msg);
    } else {
        close(con);
    }
}

void stress_handler::on_open(connection_ptr con) {
    {
        boost::lock_guard<boost::mutex> lock(m_lock);
    
        m_current_connections++;
        m_total_connections++;
    
        if (m_current_connections > m_max_connections) {
            m_max_connections = m_current_connections;
        }
        
        m_con_data[con].on_open = boost::chrono::steady_clock::now();
        m_con_data[con].status = "Open";
    }
    
    start(con);
}

void stress_handler::on_close(connection_ptr con) {
    boost::lock_guard<boost::mutex> lock(m_lock);
    
    m_current_connections--;
    
    m_con_data[con].on_close = boost::chrono::steady_clock::now();
    m_con_data[con].status = "Closed";
    
    // TODO: log close reason?
}

void stress_handler::on_fail(connection_ptr con) {
    boost::lock_guard<boost::mutex> lock(m_lock);
    
    m_failed_connections++;
    
    m_con_data[con].on_fail = boost::chrono::steady_clock::now();
    m_con_data[con].status = "Failed";
    
    // TODO: log failure reason
}

void stress_handler::start(connection_ptr con) {
    //close(con);
}

void stress_handler::close(connection_ptr con) {
    //boost::lock_guard<boost::mutex> lock(m_lock);
        
    m_con_data[con].close_sent = boost::chrono::steady_clock::now();
    m_con_data[con].status = "Closing";
    
    con->close(websocketpp::close::status::NORMAL);
    // TODO: log close reason?
}

std::string stress_handler::get_data() const {
    std::stringstream data;
    
    
    data << "{";
    
    {
        boost::lock_guard<boost::mutex> lock(m_lock);
        
        data << "\"current_connections\":" << m_current_connections;
        data << ",\"max_connections\":" << m_max_connections;
        data << ",\"total_connections\":" << m_total_connections;
        data << ",\"failed_connections\":" << m_failed_connections;
        
        data << ",\"connection_data\":[";
        
        // for each item in m_dirty
        std::string sep = "";
        std::map<connection_ptr,con_data>::const_iterator it;
        for (it = m_con_data.begin(); it != m_con_data.end(); it++) {
            data << sep << (*it).second.print();
            sep = ",";
        }
        
        data << "]";
    }
    
    data << "}";
    
    return data.str();
}

bool stress_handler::maintenance() {
    std::cout << "locking..." << std::endl;
    boost::lock_guard<boost::mutex> lock(m_lock);
    
    bool quit = true;
    
    time_point now = boost::chrono::steady_clock::now();
    
    std::cout << "found " << m_con_data.size() << " connections" << std::endl;
    
    std::map<connection_ptr,con_data>::iterator it;
    for (it = m_con_data.begin(); it != m_con_data.end(); it++) {
        if ((*it).first->get_state() != websocketpp::session::state::CLOSED) {
            quit = false;
        }
        
        connection_ptr con = (*it).first;
        con_data& data = (*it).second;
        
        std::cout << "processing " << data.id << "...";
        
        // check the connection state
        if (con->get_state() != websocketpp::session::state::OPEN) {
            std::cout << "ignored" << std::endl;
            continue;
        }
        
        boost::chrono::nanoseconds dur = now - data.on_open;
        size_t milliseconds = static_cast<size_t>(dur.count()) / 1000000;
        
        if (milliseconds > m_con_duration) {
            close(con);
        }
        std::cout << "closed" << std::endl;
    }
    
    if (quit) {
        return true;
    }
    
    
    /*std::cout << "found " << to_process.size() << " connections" << std::endl;
    
    
    
    
    std::list<connection_ptr>::iterator it2;
    for (it2 = to_process.begin(); it2 != to_process.end(); it++) {
        connection_ptr con = (*it2);
        std::map<connection_ptr,con_data>::iterator element;
        
        
        
        element = m_con_data.find(con);
        
        if (element == m_con_data.end()) {
            continue;
        }
        
        con_data& data = element->second;
        
        
        
        // check the connection state
        if (con->get_state() != websocketpp::session::state::OPEN) {
            std::cout << "ignored" << std::endl;
            continue;
        }
        
        boost::chrono::nanoseconds dur = now - data.on_open;
        size_t milliseconds = static_cast<size_t>(dur.count()) / 1000000;
        
        if (milliseconds > m_con_duration) {
            close(con);
        }
        std::cout << "closed" << std::endl;
    }*/
    
    return false;
}