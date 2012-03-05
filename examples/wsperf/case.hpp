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

#include "../../src/roles/client.hpp"
#include "../../src/websocketpp.hpp"

#include <boost/chrono.hpp>

using websocketpp::client;

namespace wsperf {

class case_handler : public client::handler {
public:
    typedef case_handler type;
    
    void start(connection_ptr con, int timeout) {
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
    }
    
    void mark() {
        m_end.push_back(boost::chrono::steady_clock::now());
    }
    
    void end(connection_ptr con) {
        uint64_t avg = 0;
        std::vector<uint64_t> avgs(10);
        double squaresum = 0;
        uint64_t stddev = 0;
        int64_t total = 0;
        double seconds = 0;
        
        // TODO: handle weird sizes and error conditions better
        if (m_end.size() > 10) {
            uint64_t last = m_start.time_since_epoch().count();
            std::vector<boost::chrono::steady_clock::time_point>::iterator it;
            for (it = m_end.begin(); it != m_end.end(); it++) {
                //(*it) -= m_start;
                m_times.push_back(it->time_since_epoch().count() - last);
                last = it->time_since_epoch().count();
            }
            
            std::sort(m_times.begin(),m_times.end());
            
            for (uint64_t i = 0; i < m_times.size(); i++) {
                avg += m_times[i];
                avgs[i/(m_times.size()/10)] += m_times[i];
                squaresum += pow(double(m_times[i]),2);
            }
            avg /= m_times.size();
            
            for (uint64_t i = 0; i < m_times.size(); i++) {
                stddev += pow(m_times[i]-avg,2);
            }
            stddev /= m_times.size();
            stddev = sqrt(stddev);
            
            for (int i = 0; i < 10; i++) {
                avgs[i] /= (m_times.size()/10);
            }
        } else {
            m_times.push_back(0);
        }
        
        total = m_end[m_end.size()-1].time_since_epoch().count() - m_start.time_since_epoch().count();
        seconds = double(total)/1000000000.0;
        
        double kbps = (double(m_bytes)/1000.0)/seconds;
        
        std::stringstream o;
        std::stringstream s;
        
        switch (m_pass) {
            case FAIL:
                o << m_name << " fails in " << seconds << "s";
                s << "{\"name\":\"" << m_name << "\",\"result\":\"fail\",\"min\":" << m_times[0] << ",\"max\":" << m_times[m_times.size()-1] << ",\"median\":" << m_times[(m_times.size()-1)/2] << ",\"avg\":" << avg << ",\"stddev\":" << stddev << ",\"sqsum\":" << squaresum << ",\"total\":" << total << ",\"KBps\":" << kbps << ",\"quantiles\":[";
                
                for (int i = 0; i < 10; i++) {
                    s << (i > 0 ? "," : "") << avgs[i];
                }
                 
                s << "]}";
                break;
            case PASS:
                o << m_name << " passes in " << seconds << "s";
                
                s << "{\"name\":\"" << m_name << "\",\"result\":\"pass\",\"min\":" << m_times[0] << ",\"max\":" << m_times[m_times.size()-1] << ",\"median\":" << m_times[(m_times.size()-1)/2] << ",\"avg\":" << avg << ",\"stddev\":" << stddev << ",\"sqsum\":" << squaresum << ",\"total\":" << total << ",\"KBps\":" << kbps << ",\"quantiles\":[";
                
                for (int i = 0; i < 10; i++) {
                    s << (i > 0 ? "," : "") << avgs[i];
                }
                 
                s << "]}";
                break;
            case TIME_OUT:
                o << m_name << " times out in " << seconds << "s";
                s << "{\"name\":\"" << m_name << "\",\"result\":\"time_out\",\"min\":" << m_times[0] << ",\"max\":" << m_times[m_times.size()-1] << ",\"median\":" << m_times[(m_times.size()-1)/2] << ",\"avg\":" << avg << ",\"stddev\":" << stddev << ",\"sqsum\":" << squaresum << ",\"total\":" << total << ",\"KBps\":" << kbps << ",\"quantiles\":[";
                
                for (int i = 0; i < 10; i++) {
                    s << (i > 0 ? "," : "") << avgs[i];
                }
                 
                s << "]}";
                break;
        }
        
        m_result = o.str();
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
    
    void on_timer(connection_ptr con,const boost::system::error_code& error) {
        if (error == boost::system::errc::operation_canceled) {
            return; // timer was canceled because test finished
        }
        
        // time out
        mark();
        m_pass = TIME_OUT;
        this->end(con);
    }
    
    void on_close(connection_ptr con) {
        //std::cout << " fails in " << len.length() << std::endl;
    }
    void on_fail(connection_ptr con) {
        std::cout << " fails to connect." << std::endl;
    }
    
    const std::string& get_result() const {
        return m_result;
    }
    const std::string& get_data() const {
        return m_data;
    }
protected:
    enum status {
        FAIL = 0,
        PASS = 1,
        TIME_OUT = 2
    };
    
    std::string                 m_name;
    std::string                 m_result;
    std::string                 m_data;
    
    status                      m_pass;
    boost::shared_ptr<boost::asio::deadline_timer> m_timer;
    
    boost::chrono::steady_clock::time_point                 m_start;
    std::vector<boost::chrono::steady_clock::time_point>    m_end;
    std::vector<uint64_t>                                   m_times;
    
    uint64_t                    m_bytes;
};

typedef boost::shared_ptr<case_handler> case_handler_ptr;

} // namespace wsperf

#endif // WSPERF_CASE_HPP