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

#ifndef WEBSOCKETPP_BROADCAST_HANDLER_HPP
#define WEBSOCKETPP_BROADCAST_HANDLER_HPP

#include "wscmd.hpp"

#include "../../src/sockets/tls.hpp"
#include "../../src/websocketpp.hpp"

#include "../../src/md5/md5.hpp"

#include <boost/date_time/posix_time/posix_time.hpp>

#include <map>
#include <set>

namespace websocketpp {
namespace broadcast {

/// this structure is used to keep track of message statistics
struct msg {
    int         id;
    size_t      sent;
    size_t      acked;
    size_t      size;
    uint64_t    time;
    
    std::string hash;
    boost::posix_time::ptime    time_sent;
};

typedef std::map<std::string,struct msg> msg_map;

template <typename endpoint_type>
class handler : public endpoint_type::handler {
public:
    typedef handler<endpoint_type> type;
    typedef boost::shared_ptr<type> ptr;
    typedef typename endpoint_type::handler_ptr handler_ptr;
    typedef typename endpoint_type::connection_ptr connection_ptr;
    
    handler() : m_nextid(0) {}
    
    void on_open(connection_ptr connection) {
        m_connections.insert(connection);
    }
    
    // this dummy tls init function will cause all TLS connections to fail.
    // TLS handling for broadcast::handler is usually done by a lobby handler.
    // If you want to use the broadcast handler alone with TLS then return the
    // appropriately filled in context here.
    boost::shared_ptr<boost::asio::ssl::context> on_tls_init() {
        return boost::shared_ptr<boost::asio::ssl::context>();
    }
    
    void on_load(connection_ptr connection, handler_ptr old_handler) {
        this->on_open(connection);
        m_lobby = old_handler;
    }
    
    void on_close(connection_ptr connection) {
        m_connections.erase(connection);
    }
    
    void on_message(connection_ptr connection,message::data_ptr msg) {
        wscmd::cmd command = wscmd::parse(msg->get_payload());
        
        std::cout << "msg: " << msg->get_payload() << std::endl;
        
        if (command.command == "ack") {
            handle_ack(connection,command);
        } else {
            broadcast_message(msg);
        }
    }
    
    void command_error(connection_ptr connection,const std::string msg) {
        connection->send("{\"type\":\"error\",\"value\":\""+msg+"\"}");
    }
    
    // ack:e3458d0aceff8b70a3e5c0afec632881=38;e3458d0aceff8b70a3e5c0afec632881=42;
    void handle_ack(connection_ptr connection,const wscmd::cmd& command) {
        wscmd::arg_list::const_iterator arg_it;
        size_t count;
        
        for (arg_it = command.args.begin(); arg_it != command.args.end(); arg_it++) {
            if (m_msgs.find(arg_it->first) == m_msgs.end()) {
                std::cout << "ack for message we didn't send" << std::endl;
                continue;
            }
            
            count = atol(arg_it->second.c_str());
            if (count == 0) {
                continue;
            }
            
            struct msg& m(m_msgs[arg_it->first]);
            
            m.acked += count;
            
            if (m.acked == m.sent) {
                m.time = get_ms(m.time_sent);
            }
        }
    }
    
    // close: - close this connection
    // close:all; close all connections
    void close_connection(connection_ptr connection) {
        if (connection){
            connection->close(close::status::NORMAL);
        } else {
            typename std::set<connection_ptr>::iterator it;
            
            for (it = m_connections.begin(); it != m_connections.end(); it++) {
                
                (*it)->close(close::status::NORMAL);
            }
        }
    }
    
    void broadcast_message(message::data_ptr msg) {
        std::string hash = md5_hash_hex(msg->get_payload());
        struct msg& new_msg(m_msgs[hash]);
        
        new_msg.id = m_nextid++;
        new_msg.hash = hash;
        new_msg.size = msg->get_payload().size();
        new_msg.time_sent = boost::posix_time::microsec_clock::local_time();
        new_msg.time = 0;
        
        typename std::set<connection_ptr>::iterator it;
        
        // broadcast to clients        
        for (it = m_connections.begin(); it != m_connections.end(); it++) {
            //(*it)->send(msg->get_payload(),(msg->get_opcode() == frame::opcode::BINARY));
            for (int i = 0; i < 10; i++) {
                (*it)->send(msg);
            }
            
        }
        new_msg.sent = m_connections.size()*10;
        new_msg.acked = 0;
    }
    
    long get_ms(boost::posix_time::ptime s) const {
        boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
        boost::posix_time::time_period period(s,now);
        return period.length().total_milliseconds();
    }
    
    // hooks for admin console
    size_t get_connection_count() const {
        return m_connections.size();
    }
    
    const msg_map& get_message_stats() const {
        return m_msgs;
    }
    
    void clear_message_stats() {
        m_msgs.empty();
    }
private:
    handler_ptr     m_lobby;
    
    int             m_nextid;
    msg_map         m_msgs;
    
    std::set<connection_ptr>    m_connections;
};

} // namespace broadcast
} // namespace websocketpp

#endif // WEBSOCKETPP_BROADCAST_HANDLER_HPP
