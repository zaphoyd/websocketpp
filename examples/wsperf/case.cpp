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

#include "case.hpp"

using wsperf::case_handler;

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
case_handler::case_handler(wscmd::cmd& cmd)
 : m_uri(extract_string(cmd,"uri")),
   m_token(extract_string(cmd,"token")),
   m_quantile_count(extract_number<size_t>(cmd,"quantile_count")),
   m_rtts(extract_bool(cmd,"rtts")),
   m_pass(RUNNING),
   m_timeout(0),
   m_bytes(0) {}

/// Starts a test by starting the timeout timer and marking the start time
void case_handler::start(connection_ptr con, uint64_t timeout) {
    if (timeout > 0) {
        m_timer.reset(new boost::asio::deadline_timer(
            con->get_io_service(),
            boost::posix_time::seconds(0))
        );
        
        m_timeout = timeout;
        
        m_timer->expires_from_now(boost::posix_time::milliseconds(m_timeout));
        m_timer->async_wait(
            boost::bind(
                &type::on_timer,
                this,
                con,
                boost::asio::placeholders::error
            )
        );
    }
    
    m_start = boost::chrono::steady_clock::now();
}

/// Marks an incremental time point
void case_handler::mark() {
    m_end.push_back(boost::chrono::steady_clock::now());
}

/// Ends a test
/* Ends a test by canceling the timeout timer, marking the end time, generating
 * statistics and closing the websocket connection.
 */
void case_handler::end(connection_ptr con) {
    std::vector<double> avgs;
    avgs.resize(m_quantile_count, 0);
    
    std::vector<double> quantiles;
    quantiles.resize(m_quantile_count, 0);
    
    double avg = 0;
    double stddev = 0;
    double total = 0;
    double seconds = 0;
    
    if (m_timeout > 0) {
        m_timer->cancel();
    }
    
    // TODO: handle weird sizes and error conditions better
    if (m_end.size() > m_quantile_count) {
        boost::chrono::steady_clock::time_point last = m_start;
        
        // convert RTTs to microsecs
        //
        
        std::vector<boost::chrono::steady_clock::time_point>::iterator it;
        for (it = m_end.begin(); it != m_end.end(); ++it) {
            boost::chrono::nanoseconds dur = *it - last;
            m_times.push_back(static_cast<double> (dur.count()) / 1000.);
            last = *it;
        }
        
        std::sort(m_times.begin(), m_times.end());
        
        size_t samples_per_quantile = m_times.size() / m_quantile_count;
        
        // quantiles
        for (size_t i = 0; i < m_quantile_count; ++i) {
            quantiles[i] = m_times[((i + 1) * samples_per_quantile) - 1];
        }
        
        // total average and quantile averages
        for (size_t i = 0; i < m_times.size(); ++i) {
            avg += m_times[i];
            avgs[i / samples_per_quantile] 
                += m_times[i] / static_cast<double>(samples_per_quantile);
        }
        
        avg /= static_cast<double> (m_times.size());
        
        // standard dev corrected for estimation from sample
        for (size_t i = 0; i < m_times.size(); ++i) {
            stddev += (m_times[i] - avg) * (m_times[i] - avg);
        }
        
        // Bessel's correction
        stddev /= static_cast<double> (m_times.size() - 1); 
        stddev = std::sqrt(stddev);
    } else {
        m_times.push_back(0);
    }
    
    boost::chrono::nanoseconds total_dur = m_end[m_end.size()-1] - m_start;
    total = static_cast<double> (total_dur.count()) / 1000.; // microsec
    seconds = total / 10000000.;
    
    std::stringstream s;
    std::string outcome;
    
    switch (m_pass) {
        case FAIL:
            outcome = "fail";
            break;
        case PASS:
            outcome = "pass";
            break;
        case TIME_OUT:
            outcome = "time_out";
            break;
        case RUNNING:
            throw case_exception("end() called from RUNNING state");
            break;
    }
    
    s << "{\"result\":\"" << outcome 
      << "\",\"min\":" << m_times[0] 
      << ",\"max\":" << m_times[m_times.size()-1] 
      << ",\"median\":" << m_times[(m_times.size()-1)/2] 
      << ",\"avg\":" << avg 
      << ",\"stddev\":" << stddev 
      << ",\"total\":" << total 
      << ",\"bytes\":" << m_bytes 
      << ",\"quantiles\":[";
      
    for (size_t i = 0; i < m_quantile_count; i++) {
        s << (i > 0 ? "," : "");
        s << "[";
        s << avgs[i] << "," << quantiles[i];
        s << "]";
    }                 
    s << "]";
    
    if (m_rtts) {
        s << ",\"rtts\":[";
        for (size_t i = 0; i < m_times.size(); i++) {
            s << (i > 0 ? "," : "") << m_times[i];
        }
        s << "]";
    };
    s << "}";
    
    m_data = s.str();
    
    con->close(websocketpp::close::status::NORMAL,"");
}

/// Fills a buffer with utf8 bytes
void case_handler::fill_utf8(std::string& data,size_t size,bool random) {
    if (random) {
        uint32_t val;
        for (size_t i = 0; i < size; i++) {
            if (i%4 == 0) {
                val = uint32_t(rand());
            }
            
            data.push_back(char(((reinterpret_cast<uint8_t*>(&val)[i%4])%95)+32));
        }
    } else {
        data.assign(size,'*');
    }
}

/// Fills a buffer with bytes
void case_handler::fill_binary(std::string& data,size_t size,bool random) {
    if (random) {
        int32_t val;
        for (size_t i = 0; i < size; i++) {
            if (i%4 == 0) {
                val = rand();
            }
            
            data.push_back((reinterpret_cast<char*>(&val))[i%4]);
        }
    } else {
        data.assign(size,'*');
    }
}

void case_handler::on_timer(connection_ptr con,const boost::system::error_code& error) {
    if (error == boost::system::errc::operation_canceled) {
        return; // timer was canceled because test finished
    }
    
    // time out
    mark();
    m_pass = TIME_OUT;
    this->end(con);
}

void case_handler::on_fail(connection_ptr con) {
    m_data = "{\"result\":\"connection_failed\"}";
}

const std::string& case_handler::get_data() const {
    return m_data;
}
const std::string& case_handler::get_token() const {
    return m_token;
}
const std::string& case_handler::get_uri() const {
    return m_uri;
}
