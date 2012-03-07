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
    
    void start(connection_ptr con, uint64_t timeout) {
        m_timer.reset(new boost::asio::deadline_timer(
            con->get_io_service(),
            boost::posix_time::seconds(0))
        );
        m_timer->expires_from_now(boost::posix_time::milliseconds(timeout));
        m_timer->async_wait(
            boost::bind(
                &type::on_timer,
                this,
                con,
                boost::asio::placeholders::error
            )
        );
        
        m_start = boost::chrono::steady_clock::now();
        m_bytes = 0;
        m_pass = RUNNING;
    }
    
    void mark() {
        m_end.push_back(boost::chrono::steady_clock::now());
    }
    
    void end(connection_ptr con) {
        /*uint64_t avg = 0;
        std::vector<uint64_t> avgs(10);
        double squaresum = 0;
        uint64_t stddev = 0;
        int64_t total = 0;
        double seconds = 0;*/
        
        std::vector<double> avgs;
        avgs.resize(m_quantile_count, 0);
        
        std::vector<double> quantiles;
        quantiles.resize(m_quantile_count, 0);
        
        double avg = 0;
        double stddev = 0;
        double total = 0;
        double seconds = 0;
        
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
    
    // Just does random ascii right now. True random UTF8 with multi-byte stuff
    // would probably be better
    void fill_utf8(std::string& data,size_t size,bool random = true) {
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
    
    void fill_binary(std::string& data,size_t size,bool random = true) {
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
    
    void on_timer(connection_ptr con,const boost::system::error_code& error) {
        if (error == boost::system::errc::operation_canceled) {
            return; // timer was canceled because test finished
        }
        
        // time out
        mark();
        m_pass = TIME_OUT;
        this->end(con);
    }
    
    void on_close(connection_ptr con) {}
    void on_fail(connection_ptr con) {
        m_data = "{\"result\":\"connection_failed\"}";
    }
    
    const std::string& get_data() const {
        return m_data;
    }
    const std::string& get_token() const {
        return m_token;
    }
    const std::string& get_uri() const {
        return m_uri;
    }
protected:
    enum status {
        FAIL = 0,
        PASS = 1,
        TIME_OUT = 2,
        RUNNING = 3
    };
    
    std::string                 m_name;
    std::string                 m_uri;
    std::string                 m_token;
    size_t                      m_quantile_count;
    bool                        m_rtts;
    std::string                 m_data;
    
    status                      m_pass;
    boost::shared_ptr<boost::asio::deadline_timer> m_timer;
    
    boost::chrono::steady_clock::time_point                 m_start;
    std::vector<boost::chrono::steady_clock::time_point>    m_end;
    std::vector<double>                                     m_times;
    
    uint64_t                    m_bytes;
};

typedef boost::shared_ptr<case_handler> case_handler_ptr;

} // namespace wsperf

#endif // WSPERF_CASE_HPP