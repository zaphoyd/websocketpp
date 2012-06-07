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

#include "stress_aggregate.hpp"

using wsperf::stress_aggregate;

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
stress_aggregate::stress_aggregate(wscmd::cmd& cmd)
 : stress_handler(cmd)
{}

void stress_aggregate::start(connection_ptr con) {
    
}

/*void stress_aggregate::on_message(connection_ptr con,websocketpp::message::data_ptr msg) {
    std::string hash = websocketpp::md5_hash_hex(msg->get_payload());
    
    boost::lock_guard<boost::mutex> lock(m_lock);
    m_msg_stats[hash]++;
}*/

/*std::string stress_aggregate::get_data() const {
    std::stringstream data;
    
    std::string sep = "";
    
    data << "{";
    
    std::map<std::string,size_t>::iterator it;
    
    {
        boost::lock_guard<boost::mutex> lock(m_lock);
        for (it = m_msg_stats.begin(); it != m_msg_stats.end(); it++) {
            data << sep << "\"" << (*it)->first << "\":" << (*it)->second;
            sep = ",";
        }
    }
    
    data << "}";
    
    return data;
}*/

