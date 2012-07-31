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

#include "../../src/endpoint.hpp"
#include "../../src/roles/client.hpp"
#include "../../src/md5/md5.hpp"

#include <boost/thread.hpp>

#include <iostream>

// PLATFORM SPECIFIC STUFF
#include <unistd.h>
#include <sys/resource.h>
#include <time.h>

int msleep(unsigned long milisec);

int msleep(unsigned long milisec)
{
    struct timespec req={0};
    time_t sec=(int)(milisec/1000);
    milisec=milisec-(sec*1000);
    req.tv_sec=sec;
    req.tv_nsec=milisec*1000000L;
    while(nanosleep(&req,&req)==-1)
        continue;
    return 1;
}
// END PLATFORM SPECIFIC STUFF


typedef websocketpp::endpoint<websocketpp::role::client,websocketpp::socket::plain> plain_endpoint_type;
typedef plain_endpoint_type::handler_ptr plain_handler_ptr;
typedef plain_endpoint_type::connection_ptr connection_ptr;

class stress_client_handler : public plain_endpoint_type::handler {
public:
    typedef stress_client_handler type;
    typedef plain_endpoint_type::connection_ptr connection_ptr;
    
    stress_client_handler(int num_connections) 
     : m_connections_max(num_connections),
       m_connections_cur(0) {
        
    }
    
    void on_open(connection_ptr connection) {
        if (!m_timer) {
            m_timer.reset(new boost::asio::deadline_timer(connection->get_io_service(),boost::posix_time::seconds(0)));
            m_timer->expires_from_now(boost::posix_time::milliseconds(250));
            m_timer->async_wait(boost::bind(&type::on_timer,this,connection,boost::asio::placeholders::error));
        }
        
        m_connections_cur++;
        
        if (m_connections_cur == m_connections_max) {
            boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
            boost::posix_time::time_period period(m_start_time,now);
            int ms = period.length().total_milliseconds();
            
            std::cout << "Started " << m_connections_cur << " in " << ms << "ms" << " (" << m_connections_cur/(ms/1000.0) << "/s)" << std::endl;
        }
    }
    
    void on_message(connection_ptr connection, websocketpp::message::data::ptr msg) {
        //std::cout << "got message of size: " << msg->get_payload().size() << std::endl;
        m_msg_stats[websocketpp::md5_hash_hex(msg->get_payload())]++;
        
        if (m_msg_stats[websocketpp::md5_hash_hex(msg->get_payload())] == m_connections_max) {
            send_stats_update(connection);
        }
    }
    
    void on_fail(connection_ptr connection) {
        std::cout << "connection failed" << std::endl;
    }
    
    void on_timer(connection_ptr connection,const boost::system::error_code& error) {
        if (error) {
            std::cout << "on_timer error" << std::endl;
            return;
        }
        
        send_stats_update(connection);
                
        m_timer->expires_from_now(boost::posix_time::milliseconds(250));
        m_timer->async_wait(boost::bind(&type::on_timer,this,connection,boost::asio::placeholders::error));
    }
    
    void on_close(connection_ptr connection) {
        m_timer->cancel();
    }
    
    boost::posix_time::ptime    m_start_time;
private:    
    void send_stats_update(connection_ptr connection) {
        if (m_msg_stats.empty()) {
            return;
        }
        // example: `ack:e3458d0aceff8b70a3e5c0afec632881=38;e3458d0aceff8b70a3e5c0afec632881=42;`
        std::stringstream msg;
        msg << "ack:";
        
        std::map<std::string,size_t>::iterator it;
        
        for (it = m_msg_stats.begin(); it != m_msg_stats.end(); it++) {
            msg << (*it).first << "=" << (*it).second << ";";
        }
        
        std::cout << "sending " << msg.str() << std::endl;
        connection->send(msg.str());
        
        m_msg_stats.clear();
    }
    
    unsigned int m_connections_max;
    unsigned int m_connections_cur;
    std::map<std::string,size_t>                    m_msg_stats;
    boost::shared_ptr<boost::asio::deadline_timer>  m_timer;
};


int main(int argc, char* argv[]) {
    std::string uri = "ws://localhost:9002/";
    int num_connections = 100;
    int batch_size = 25;
    int delay_ms = 16;

    if (argc > 1) {
        if (argc != 5) {
            std::cout << "Usage: `echo_client test_url num_connections batch_size delay_ms`" << std::endl;
        } else {
            uri = argv[1];
            num_connections = atoi(argv[2]);
            batch_size = atoi(argv[3]);
            delay_ms = atoi(argv[4]);
        }
    }
        
    // 12288 is max OS X limit without changing kernal settings
    const rlim_t ideal_size = 200+num_connections;
    rlim_t old_size;
    rlim_t old_max;
    
    struct rlimit rl;
    int result;
    
    result = getrlimit(RLIMIT_NOFILE, &rl);
    if (result == 0) {
        //std::cout << "System FD limits: " << rl.rlim_cur << " max: " << rl.rlim_max << std::endl;
        
        old_size = rl.rlim_cur;
        old_max = rl.rlim_max;
        
        if (rl.rlim_cur < ideal_size) {
            std::cout << "Attempting to raise system file descriptor limit from " << rl.rlim_cur << " to " << ideal_size << std::endl;
            rl.rlim_cur = ideal_size;
            
            if (rl.rlim_max < ideal_size) {
                rl.rlim_max = ideal_size;
            }
            
            result = setrlimit(RLIMIT_NOFILE, &rl);
            
            if (result == 0) {
                std::cout << "Success" << std::endl;
            } else if (result == EPERM) {
                std::cout << "Failed. This server will be limited to " << old_size << " concurrent connections. Error code: Insufficient permissions. Try running process as root. system max: " << old_max << std::endl;
            } else {
                std::cout << "Failed. This server will be limited to " << old_size << " concurrent connections. Error code: " << errno << " system max: " << old_max << std::endl;
            }
        }
    }
    
    try {
        plain_handler_ptr handler(new stress_client_handler(num_connections));
        plain_endpoint_type endpoint(handler);
        
        endpoint.alog().unset_level(websocketpp::log::alevel::ALL);
        endpoint.elog().set_level(websocketpp::log::elevel::ALL);
        
        //endpoint.alog().set_level(websocketpp::log::alevel::DEVEL);
        //endpoint.alog().set_level(websocketpp::log::alevel::DEBUG_CLOSE);
        
        std::set<connection_ptr> connections;
        
        connections.insert(endpoint.connect(uri));
        
        boost::thread t(boost::bind(&plain_endpoint_type::run, &endpoint, false));
        
        std::cout << "launching " << num_connections << " connections to " << uri << " in batches of " << batch_size << std::endl;
        
        boost::dynamic_pointer_cast<stress_client_handler>(handler)->m_start_time = boost::posix_time::microsec_clock::local_time();
        for (int i = 0; i < num_connections-1; i++) {
            if (i % batch_size == 0) {
                //sleep(1);
                msleep(delay_ms);
            }
            connections.insert(endpoint.connect(uri));
        }
        
        std::cout << "complete" << std::endl;
        
        t.join();
        
        std::cout << "done" << std::endl;
        
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    
    return 0;
}
