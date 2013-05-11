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

#ifndef WEBSOCKETPP_TRANSPORT_SECURITY_NONE_HPP
#define WEBSOCKETPP_TRANSPORT_SECURITY_NONE_HPP

#include <websocketpp/transport/asio/security/base.hpp>

#include <boost/asio.hpp>

#include <iostream>
#include <string>

namespace websocketpp {
namespace transport {
namespace asio {
namespace basic_socket {

typedef lib::function<void(connection_hdl,boost::asio::ip::tcp::socket&)> 
    socket_init_handler;

/// Basic Boost ASIO connection socket component
/**
 * transport::asio::basic_socket::connection impliments a connection socket
 * component using Boost ASIO ip::tcp::socket.
 */
class connection {
public:
    /// Type of this connection socket component
    typedef connection type;
    /// Type of a shared pointer to this connection socket component
    typedef lib::shared_ptr<type> ptr;

    /// Type of a pointer to the ASIO io_service being used
    typedef boost::asio::io_service* io_service_ptr;
    /// Type of a shared pointer to the socket being used.
    typedef lib::shared_ptr<boost::asio::ip::tcp::socket> socket_ptr;
    
    explicit connection() : m_state(UNINITIALIZED) {
        //std::cout << "transport::asio::basic_socket::connection constructor" 
        //          << std::endl; 
    }
    
    /// Check whether or not this connection is secure
    /**
     * @return Wether or not this connection is secure
     */
    bool is_secure() const {
        return false;
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

    /// Retrieve a pointer to the underlying socket
    /**
     * This is used internally. It can also be used to set socket options, etc
     */
    boost::asio::ip::tcp::socket& get_socket() {
        return *m_socket;
    }
    
    /// Retrieve a pointer to the underlying socket
    /**
     * This is used internally.
     */
    boost::asio::ip::tcp::socket& get_next_layer() {
        return *m_socket;
    }
    
    /// Retrieve a pointer to the underlying socket
    /**
     * This is used internally. It can also be used to set socket options, etc
     */
    boost::asio::ip::tcp::socket& get_raw_socket() {
        return *m_socket;
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
        boost::asio::ip::tcp::endpoint ep = m_socket->remote_endpoint(bec);
        
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
     * @param strand A shared pointer to the connection's asio strand
     * @param is_server Whether or not the endpoint is a server or not.
     */
    lib::error_code init_asio (io_service_ptr service, bool is_server) {
        if (m_state != UNINITIALIZED) {
            return socket::make_error_code(socket::error::invalid_state);
        }
        
        m_socket.reset(new boost::asio::ip::tcp::socket(*service));
        
        m_state = READY;
        
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
        if (m_state != READY) {
            callback(socket::make_error_code(socket::error::invalid_state));
            return;
        }
        
        if (m_socket_init_handler) {
            m_socket_init_handler(m_hdl,*m_socket);
        }
        
        m_state = READING;
        
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
        callback(lib::error_code());
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
    
    /// Cancel all async operations on this socket
    void cancel_socket() {
        m_socket->cancel();
    }
    
    void async_shutdown(socket_shutdown_handler h) {
        boost::system::error_code ec;
        m_socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both,ec);
        h(ec);
    }
    
    lib::error_code get_ec() const {
        return lib::error_code();
    }
private:
    enum state {
        UNINITIALIZED = 0,
        READY = 1,
        READING = 2
    };
    
    socket_ptr          m_socket;
    state               m_state;
        
    connection_hdl      m_hdl;
    socket_init_handler m_socket_init_handler;
};

/// Basic ASIO endpoint socket component
/**
 * transport::asio::basic_socket::endpoint impliments an endpoint socket
 * component that uses Boost ASIO's ip::tcp::socket.
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
        return false;
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
        return lib::error_code();
    }
private:
    socket_init_handler m_socket_init_handler;
};

} // namespace basic_socket
} // namespace asio
} // namespace transport
} // namespace websocketpp

#endif // WEBSOCKETPP_TRANSPORT_SECURITY_NONE_HPP
