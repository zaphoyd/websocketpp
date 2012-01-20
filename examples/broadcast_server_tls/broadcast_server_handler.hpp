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

#ifndef WEBSOCKETPP_BROADCAST_SERVER_HANDLER_HPP
#define WEBSOCKETPP_BROADCAST_SERVER_HANDLER_HPP

#include "../../src/sockets/tls.hpp"
#include "../../src/websocketpp.hpp"

#include "broadcast_handler.hpp"
#include "broadcast_admin_handler.hpp"

#include <boost/date_time/posix_time/posix_time.hpp>

namespace websocketpp {
namespace broadcast {
    
template <typename endpoint_type>
class server_handler : public endpoint_type::handler {
public:
    typedef server_handler<endpoint_type> type;
    typedef boost::shared_ptr<type> ptr;
    typedef typename endpoint_type::handler_ptr handler_ptr;
    typedef typename admin_handler<endpoint_type>::ptr admin_handler_ptr;
    typedef typename handler<endpoint_type>::ptr broadcast_handler_ptr;
    typedef typename endpoint_type::connection_ptr connection_ptr;
    
    server_handler();
    
    std::string get_password() const {
        return "test";
    }
    
    boost::shared_ptr<boost::asio::ssl::context> on_tls_init() {
        // create a tls context, init, and return.
        boost::shared_ptr<boost::asio::ssl::context> context(new boost::asio::ssl::context(boost::asio::ssl::context::tlsv1));
        try {
            context->set_options(boost::asio::ssl::context::default_workarounds |
                                 boost::asio::ssl::context::no_sslv2 |
                                 boost::asio::ssl::context::single_dh_use);
            context->set_password_callback(boost::bind(&type::get_password, this));
            context->use_certificate_chain_file("../../src/ssl/server.pem");
            context->use_private_key_file("../../src/ssl/server.pem", boost::asio::ssl::context::pem);
            context->use_tmp_dh_file("../../src/ssl/dh512.pem");
        } catch (std::exception& e) {
            std::cout << e.what() << std::endl;
        }
        return context;
    }
    
    void validate(connection_ptr connection) {}
    
    void on_open(connection_ptr connection) {
        if (connection->get_resource() == "/admin") {
            connection->set_handler(m_admin_handler);
        } else {
            connection->set_handler(m_broadcast_handler);
        }
    }
    
    void on_unload(connection_ptr connection, handler_ptr new_handler) {
        
    }
    
    void on_close(connection_ptr connection) {}
    
    void on_message(connection_ptr connection,websocketpp::message::data_ptr msg) {}
    
    void http(connection_ptr connection);
    
    void on_fail(connection_ptr connection) {
        std::cout << "connection failed" << std::endl;
    }
    
    // utility
    
    handler_ptr get_broadcast_handler() {
        return m_broadcast_handler;
    }
    
private:
    admin_handler_ptr       m_admin_handler;
    broadcast_handler_ptr   m_broadcast_handler;
};

} // namespace broadcast
} // namespace websocketpp




namespace websocketpp {
namespace broadcast {

template <class endpoint>
server_handler<endpoint>::server_handler() 
 : m_admin_handler(new admin_handler<endpoint>()),
   m_broadcast_handler(new handler<endpoint>())
{
    m_admin_handler->track(m_broadcast_handler);
}

template <class endpoint>
void server_handler<endpoint>::http(connection_ptr connection) {
    std::stringstream foo;
    
    foo << "<html><body><p>" 
        << m_broadcast_handler->get_connection_count()
        << " current connections.</p></body></html>";
    
    connection->set_body(foo.str());
}

} // namespace broadcast
} // namespace websocketpp

#endif // WEBSOCKETPP_BROADCAST_SERVER_HANDLER_HPP
