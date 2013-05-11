/*
 * Copyright (c) 2013, Peter Thorson. All rights reserved.
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

#ifndef WEBSOCKETPP_TRANSPORT_SECURITY_TLS_HPP
#define WEBSOCKETPP_TRANSPORT_SECURITY_TLS_HPP

#include <websocketpp/transport/asio/security/base.hpp>
#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/common/functional.hpp>
#include <websocketpp/common/memory.hpp>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/system/error_code.hpp>

#include <iostream>
#include <string>

namespace websocketpp {
namespace transport {
namespace asio {
namespace tls_socket {

typedef lib::function<void(connection_hdl,boost::asio::ssl::stream<
    boost::asio::ip::tcp::socket>&)> socket_init_handler;
typedef lib::function<lib::shared_ptr<boost::asio::ssl::context>(connection_hdl)>
    tls_init_handler;

/// TLS enabled Boost ASIO connection socket component
/**
 * transport::asio::tls_socket::connection impliments a secure connection socket
 * component that uses Boost ASIO's ssl::stream to wrap an ip::tcp::socket.
 */
class connection {
public:
    /// Type of this connection socket component
    typedef connection type;
    /// Type of a shared pointer to this connection socket component
    typedef lib::shared_ptr<type> ptr;
    
    /// Type of the ASIO socket being used
    typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket_type;
    /// Type of a shared pointer to the ASIO socket being used
    typedef lib::shared_ptr<socket_type> socket_ptr;
    /// Type of a pointer to the ASIO io_service being used
    typedef boost::asio::io_service* io_service_ptr;
    /// Type of a shared pointer to the ASIO TLS context being used
    typedef lib::shared_ptr<boost::asio::ssl::context> context_ptr;
    
    typedef boost::system::error_code boost_error;
    
    explicit connection() {
        //std::cout << "transport::asio::tls_socket::connection constructor" 
        //          << std::endl; 
    }
    
    /// Check whether or not this connection is secure
    /**
     * @return Wether or not this connection is secure
     */
    bool is_secure() const {
        return true;
    }
    
    /// Retrieve a pointer to the underlying socket
    /**
     * This is used internally. It can also be used to set socket options, etc
     */
    socket_type::lowest_layer_type& get_raw_socket() {
        return m_socket->lowest_layer();
    }
    
    /// Retrieve a pointer to the layer below the ssl stream
    /**
     * This is used internally.
     */
    socket_type::next_layer_type& get_next_layer() {
        return m_socket->next_layer();
    }
    
    /// Retrieve a pointer to the wrapped socket
    /**
     * This is used internally.
     */
    socket_type& get_socket() {
        return *m_socket;
    }

    /// Set the socket initialization handler
    /**
     * The socket initialization handler is called after the socket object is
     * created but before it is used. This gives the application a chance to
     * set any ASIO socket options it needs.
     *
     * @param h The new socket_init_handler
     */
    void set_socket_init_handler(socket_init_handler h) {
        m_socket_init_handler = h;
    }

    /// Set TLS init handler
    /**
     * The tls init handler is called when needed to request a TLS context for
     * the library to use. A TLS init handler must be set and it must return a
     * valid TLS context in order for this endpoint to be able to initialize 
     * TLS connections
     *
     * @param h The new tls_init_handler
     */
    void set_tls_init_handler(tls_init_handler h) {
        m_tls_init_handler = h;
    }
    
    /// Get the remote endpoint address
    /**
     * The iostream transport has no information about the ultimate remote 
     * endpoint. It will return the string "iostream transport". To indicate 
     * this.
     *
     * TODO: allow user settable remote endpoint addresses if this seems useful
     *
     * @return A string identifying the address of the remote endpoint
     */
    std::string get_remote_endpoint(lib::error_code &ec) const {
        std::stringstream s;
        
        boost::system::error_code bec;
        boost::asio::ip::tcp::endpoint ep = m_socket->lowest_layer().remote_endpoint(bec);
        
        if (bec) {
            ec = error::make_error_code(error::pass_through);
            s << "Error getting remote endpoint: " << bec 
               << " (" << bec.message() << ")";
            return s.str();
        } else {
            ec = lib::error_code();
            s << ep;
            return s.str();
        }
    }
protected:
    /// Perform one time initializations
    /**
     * init_asio is called once immediately after construction to initialize
     * boost::asio components to the io_service
     *
     * @param service A pointer to the endpoint's io_service
     * @param is_server Whether or not the endpoint is a server or not.
     */
    lib::error_code init_asio (io_service_ptr service, bool is_server) {
        if (!m_tls_init_handler) {
            return socket::make_error_code(socket::error::missing_tls_init_handler);
        }
        m_context = m_tls_init_handler(m_hdl);
        
        if (!m_context) {
            return socket::make_error_code(socket::error::invalid_tls_context);
        }
        m_socket.reset(new socket_type(*service,*m_context));
        
        m_io_service = service;
        m_is_server = is_server;
        
        return lib::error_code();
    }
    
    /// Pre-initialize security policy
    /**
     * Called by the transport after a new connection is created to initialize
     * the socket component of the connection. This method is not allowed to 
     * write any bytes to the wire. This initialization happens before any 
     * proxies or other intermediate wrappers are negotiated.
     * 
     * @param callback Handler to call back with completion information
     */
    void pre_init(init_handler callback) {
        if (m_socket_init_handler) {
            m_socket_init_handler(m_hdl,get_socket());
        }
        
        callback(lib::error_code());
    }
    
    /// Post-initialize security policy
    /**
     * Called by the transport after all intermediate proxies have been 
     * negotiated. This gives the security policy the chance to talk with the
     * real remote endpoint for a bit before the websocket handshake.
     * 
     * @param callback Handler to call back with completion information
     */
    void post_init(init_handler callback) {
        m_ec = socket::make_error_code(socket::error::tls_handshake_timeout);
        
        // TLS handshake
        m_socket->async_handshake(
            get_handshake_type(),
            lib::bind(
                &type::handle_init,
                this,
                callback,
                lib::placeholders::_1
            )
        );
    }
    
    /// Sets the connection handle
    /**
     * The connection handle is passed to any handlers to identify the 
     * connection
     *
     * @param hdl The new handle
     */
    void set_handle(connection_hdl hdl) {
        m_hdl = hdl;
    }
    
    void handle_init(init_handler callback, const 
        boost::system::error_code& ec)
    {
        if (ec) {
            m_ec = socket::make_error_code(socket::error::pass_through);
        } else {
            m_ec = lib::error_code();
        }
        
        callback(m_ec);
    }
    
    lib::error_code get_ec() const {
        return m_ec;
    }
    
    /// Cancel all async operations on this socket
    void cancel_socket() {
        get_raw_socket().cancel();
    }
    
    void async_shutdown(socket_shutdown_handler callback) {
        m_socket->async_shutdown(callback);
    }
private:
    socket_type::handshake_type get_handshake_type() {
        if (m_is_server) {
            return boost::asio::ssl::stream_base::server;
        } else {
            return boost::asio::ssl::stream_base::client;
        }
    }
    
    io_service_ptr      m_io_service;
    context_ptr         m_context;
    socket_ptr          m_socket;
    bool                m_is_server;
    
    lib::error_code     m_ec;
    
    connection_hdl      m_hdl;
    socket_init_handler m_socket_init_handler;
    tls_init_handler    m_tls_init_handler;
};

/// TLS enabled Boost ASIO endpoint socket component
/**
 * transport::asio::tls_socket::endpoint impliments a secure endpoint socket
 * component that uses Boost ASIO's ssl::stream to wrap an ip::tcp::socket.
 */
class endpoint {
public:
    /// The type of this endpoint socket component
    typedef endpoint type;

    /// The type of the corresponding connection socket component
    typedef connection socket_con_type;
    /// The type of a shared pointer to the corresponding connection socket
    /// component.
    typedef socket_con_type::ptr socket_con_ptr;

    explicit endpoint() {}

    /// Checks whether the endpoint creates secure connections
    /**
     * @return Wether or not the endpoint creates secure connections
     */
    bool is_secure() const {
        return true;
    }

    /// Set socket init handler
    /**
     * The socket init handler is called after a connection's socket is created
     * but before it is used. This gives the end application an opportunity to 
     * set asio socket specific parameters.
     *
     * @param h The new socket_init_handler
     */
    void set_socket_init_handler(socket_init_handler h) {
        m_socket_init_handler = h;
    }
    
    /// Set TLS init handler
    /**
     * The tls init handler is called when needed to request a TLS context for
     * the library to use. A TLS init handler must be set and it must return a
     * valid TLS context in order for this endpoint to be able to initialize 
     * TLS connections
     *
     * @param h The new tls_init_handler
     */
    void set_tls_init_handler(tls_init_handler h) {
        m_tls_init_handler = h;
    }
protected:
    /// Initialize a connection
    /**
     * Called by the transport after a new connection is created to initialize
     * the socket component of the connection.
     * 
     * @param scon Pointer to the socket component of the connection
     *
     * @return Error code (empty on success)
     */
    lib::error_code init(socket_con_ptr scon) {
        scon->set_socket_init_handler(m_socket_init_handler);
        scon->set_tls_init_handler(m_tls_init_handler);
        return lib::error_code();
    }

private:
    socket_init_handler m_socket_init_handler;
    tls_init_handler m_tls_init_handler;
};

} // namespace tls_socket
} // namespace asio
} // namespace transport
} // namespace websocketpp

#endif // WEBSOCKETPP_TRANSPORT_SECURITY_TLS_HPP
