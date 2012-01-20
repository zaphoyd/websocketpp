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

#ifndef WEBSOCKETPP_BROADCAST_ADMIN_HANDLER_HPP
#define WEBSOCKETPP_BROADCAST_ADMIN_HANDLER_HPP

#include "../../src/sockets/tls.hpp"
#include "../../src/websocketpp.hpp"

#include "broadcast_handler.hpp"

#include <boost/date_time/posix_time/posix_time.hpp>

#include <map>
#include <set>
#include <sstream>

namespace websocketpp {
namespace broadcast {

template <typename endpoint_type>
class admin_handler : public endpoint_type::handler {
public:
    typedef admin_handler<endpoint_type> type;
    typedef boost::shared_ptr<type> ptr;
    typedef typename endpoint_type::handler_ptr handler_ptr;
    typedef typename handler<endpoint_type>::ptr broadcast_handler_ptr;
    typedef typename endpoint_type::connection_ptr connection_ptr;
    
    admin_handler()
     : m_epoch(boost::posix_time::time_from_string("1970-01-01 00:00:00.000")) 
     {}
    
    void on_open(connection_ptr connection) {
        if (!m_timer) {
            m_timer.reset(new boost::asio::deadline_timer(connection->get_io_service(),boost::posix_time::seconds(0)));
            m_timer->expires_from_now(boost::posix_time::milliseconds(250));
            m_timer->async_wait(boost::bind(&type::on_timer,this,boost::asio::placeholders::error));
        }
    
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
    
    void track(broadcast_handler_ptr target) {
        m_broadcast_handler = target;
    }
    
    void on_close(connection_ptr connection) {
        m_connections.erase(connection);
    }
    
    void on_message(connection_ptr connection,websocketpp::message::data_ptr msg) {
        typename std::set<connection_ptr>::iterator it;
        
        wscmd::cmd command = wscmd::parse(msg->get_payload());
        
        if (command.command == "close") {
            handle_close(connection,command);
        } else {
            command_error(connection,"Invalid Command");
        }
    }
    
    void command_error(connection_ptr connection,const std::string msg) {
        std::string str = "{\"type\":\"error\",\"value\":\""+msg+"\"}";
        connection->send(str);
    }
    
    // close: - close this connection
    // close:all; close all connections
    void handle_close(connection_ptr connection,const wscmd::cmd& command) {
        if (!m_broadcast_handler) {
            // Unable to connect to local broadcast handler
            return;
        }
        
        m_broadcast_handler->close_connection(connection_ptr());
    }
    
    long get_ms(boost::posix_time::ptime s) const {
        boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
        boost::posix_time::time_period period(s,now);
        return period.length().total_milliseconds();
    }
    
    void on_timer(const boost::system::error_code& error) {
        if (!m_broadcast_handler) {
            // Unable to connect to local broadcast handler
            return;
        }
        
        if (m_connections.size() > 0) {
            long milli_seconds = get_ms(m_epoch);
            
            std::stringstream update;
            update << "{\"type\":\"stats\""
                   << ",\"timestamp\":" << milli_seconds
                   << ",\"connections\":" << m_broadcast_handler->get_connection_count()
                   << ",\"admin_connections\":" << m_connections.size()
                   << ",\"messages\":[";
            
            const msg_map& m = m_broadcast_handler->get_message_stats();
            
            msg_map::const_iterator msg_it;
            msg_map::const_iterator last = m.end();
            if (m.size() > 0) {
                last--;
            }
            
            for (msg_it = m.begin(); msg_it != m.end(); msg_it++) {
                update << "{\"id\":" << (*msg_it).second.id
                       << ",\"hash\":\"" << (*msg_it).second.hash << "\""
                       << ",\"sent\":" << (*msg_it).second.sent
                       << ",\"acked\":" << (*msg_it).second.acked
                       << ",\"size\":" << (*msg_it).second.size
                       << ",\"time\":" << (*msg_it).second.time
                       << "}" << (msg_it == last ? "" : ",");
            }
            
            update << "]}";
            
            m_broadcast_handler->clear_message_stats();
            
            typename std::set<connection_ptr>::iterator it;
            
            websocketpp::message::data_ptr msg = (*m_connections.begin())->get_data_message();
            
            if (msg) {
                msg->reset(frame::opcode::TEXT);
                
                msg->set_payload(update.str());
                
                for (it = m_connections.begin(); it != m_connections.end(); it++) {
                    (*it)->send(msg);
                }
            } else {
                // error no avaliable message buffers
            }
        }
        
        m_timer->expires_from_now(boost::posix_time::milliseconds(250));
        m_timer->async_wait(
            boost::bind(
                &type::on_timer,
                this,
                boost::asio::placeholders::error
            )
        );
    }
private:
    handler_ptr      m_lobby;
    broadcast_handler_ptr   m_broadcast_handler;
    
    std::set<connection_ptr>    m_connections;
    boost::posix_time::ptime    m_epoch;
    
    boost::shared_ptr<boost::asio::deadline_timer> m_timer;
};
    
} // namespace broadcast
} // namespace websocketpp

#endif // WEBSOCKETPP_BROADCAST_ADMIN_HANDLER_HPP
