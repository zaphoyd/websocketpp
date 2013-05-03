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

#ifndef WEBSOCKETPP_TRANSPORT_ASIO_CON_HPP
#define WEBSOCKETPP_TRANSPORT_ASIO_CON_HPP

#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/functional.hpp>
#include <websocketpp/common/connection_hdl.hpp>

#include <websocketpp/transport/asio/base.hpp>
#include <websocketpp/transport/base/connection.hpp>

#include <websocketpp/base64/base64.hpp>
#include <websocketpp/error.hpp>

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>

#include <sstream>
#include <vector>

namespace websocketpp {
namespace transport {
namespace asio {

typedef lib::function<void(connection_hdl)> tcp_init_handler;

/// Boost Asio based connection transport component
/**
 * transport::asio::connection impliments a connection transport component using
 * Boost ASIO that works with the transport::asio::endpoint endpoint transport
 * component.
 */
template <typename config>
class connection : public config::socket_type::socket_con_type {
public:
    /// Type of this connection transport component
    typedef connection<config> type;
    /// Type of a shared pointer to this connection transport component
    typedef lib::shared_ptr<type> ptr;

    /// Type of the socket connection component
    typedef typename config::socket_type::socket_con_type socket_con_type;
    /// Type of a shared pointer to the socket connection component
    typedef typename socket_con_type::ptr socket_con_ptr;
    /// Type of this transport's access logging policy
    typedef typename config::alog_type alog_type;
    /// Type of this transport's error logging policy
    typedef typename config::elog_type elog_type;
    
    typedef typename config::request_type request_type;
    typedef typename request_type::ptr request_ptr;
    typedef typename config::response_type response_type;
    typedef typename response_type::ptr response_ptr;
    
    /// Type of a pointer to the ASIO io_service being used
    typedef boost::asio::io_service* io_service_ptr;    

    // generate and manage our own io_service
    explicit connection(bool is_server, alog_type& alog, elog_type& elog)
      : m_is_server(is_server)
      , m_alog(alog)
      , m_elog(elog)
    {
        m_alog.write(log::alevel::devel,"asio con transport constructor");
    }
    
    bool is_secure() const {
        return socket_con_type::is_secure();
    }
    
    /// Finish constructing the transport
    /**
     * init_asio is called once immediately after construction to initialize
     * boost::asio components to the io_service
     *
     * TODO: this method is not protected because the endpoint needs to call it.
     * need to figure out if there is a way to friend the endpoint safely across
     * different compilers.
     */
    void init_asio (io_service_ptr io_service) {
        // do we need to store or use the io_service at this level?
        m_io_service = io_service;

        //m_strand.reset(new boost::asio::strand(*io_service));
        
        socket_con_type::init_asio(io_service, m_is_server);
    }

    void set_tcp_init_handler(tcp_init_handler h) {
        m_tcp_init_handler = h;
    }
    
    void set_proxy(const std::string & proxy) {
        m_proxy = proxy;
        m_proxy_data.reset(new proxy_data());
    }
    void set_proxy_basic_auth(const std::string & u, const std::string & p) {
        if (m_proxy_data) {
            std::string val = "Basic "+base64_encode(u + ": " + p);
            m_proxy_data->req.replace_header("Proxy-Authorization",val);
        } else {
            // TODO: should we throw errors with invalid stuff here or just
            // silently ignore?
        }
    }
    
    const std::string & get_proxy() const {
        return m_proxy;
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
    std::string get_remote_endpoint() const {
        lib::error_code ec;
        
        std::string ret = socket_con_type::get_remote_endpoint(ec);
        
        if (ec) {
            m_elog.write(log::elevel::info,ret);
            return "Unknown";
        } else {
            return ret;
        }
    }
    
    /// Get the connection handle
    connection_hdl get_handle() const {
        return m_connection_hdl;
    }
    
    /// initialize the proxy buffers and http parsers
    /**
     *
     * @param authority The address of the server we want the proxy to tunnel to
     * in the format of a URI authority (host:port)
     */
    lib::error_code proxy_init(const std::string & authority) {
        if (!m_proxy_data) {
            return websocketpp::error::make_error_code(websocketpp::error::invalid_state);
        }
        m_proxy_data->req.set_version("HTTP/1.1");
        m_proxy_data->req.set_method("CONNECT");

        m_proxy_data->req.set_uri(authority);
        m_proxy_data->req.replace_header("Host",authority);

        return lib::error_code();
    }

protected:
    /// Initialize transport for reading
    /**
     * init_asio is called once immediately after construction to initialize
     * boost::asio components to the io_service
     */
    void init(init_handler callback) {
        m_alog.write(log::alevel::devel,"asio connection init");
        
        socket_con_type::init(
            lib::bind(
                &type::handle_init,
                this,
                callback,
                lib::placeholders::_1
            )
        );
    }
    
    void handle_init(init_handler callback, const lib::error_code& ec) {
        if (m_tcp_init_handler) {
            m_tcp_init_handler(m_connection_hdl);
        }
        
        if (ec) {
            callback(ec);
        }
        
        // If no error, and we are an insecure connection with a proxy set
        // issue a proxy connect.
        if (!m_proxy.empty()) {
            proxy_write(callback);
        } else {
            callback(ec);
        }
    }
    
    void proxy_write(init_handler callback) {
        if (!m_proxy_data) {
            m_elog.write(log::elevel::library,
                "assertion failed: !m_proxy_data in asio::connection::proxy_write");
            callback(make_error_code(error::general));
            return;
        }
        
        m_proxy_data->write_buf = m_proxy_data->req.raw();
        
        m_bufs.push_back(boost::asio::buffer(m_proxy_data->write_buf.data(),
                                             m_proxy_data->write_buf.size()));
        
        m_alog.write(log::alevel::devel,m_proxy_data->write_buf);

        boost::asio::async_write(
            socket_con_type::get_next_layer(),
            m_bufs,
            lib::bind(
                &type::handle_proxy_write,
                this,
                callback,
                lib::placeholders::_1
            )
        );
    }
    
    void handle_proxy_write(init_handler callback, const 
        boost::system::error_code& ec)
    {
        m_bufs.clear();

        if (ec) {
            m_elog.write(log::elevel::info,
                "asio handle_proxy_write error: "+ec.message());
            callback(make_error_code(error::pass_through)); 
        } else {
            proxy_read(callback);
        }
    }
    
    void proxy_read(init_handler callback) {
        if (!m_proxy_data) {
            m_elog.write(log::elevel::library,
                "assertion failed: !m_proxy_data in asio::connection::proxy_read");
            callback(make_error_code(error::general));
            return;
        }
        
        boost::asio::async_read_until(
            socket_con_type::get_next_layer(),
            m_proxy_data->read_buf,
            "\r\n\r\n",
            lib::bind(
                &type::handle_proxy_read,
                this,
                callback,
                lib::placeholders::_1,
                lib::placeholders::_2
            )
        );
    }
    
    void handle_proxy_read(init_handler callback, const 
        boost::system::error_code& ec, size_t bytes_transferred)
    {
        if (ec) {
            m_elog.write(log::elevel::info,
                "asio handle_proxy_read error: "+ec.message());
            callback(make_error_code(error::pass_through)); 
        } else {
            if (!m_proxy_data) {
                m_elog.write(log::elevel::library,
                    "assertion failed: !m_proxy_data in asio::connection::handle_proxy_read");
                callback(make_error_code(error::general));
                return;
            }
            
            std::istream input(&m_proxy_data->read_buf);

            m_proxy_data->res.consume(input);
            
            if (!m_proxy_data->res.headers_ready()) {
                // we read until the headers were done in theory but apparently
                // they aren't. Internal endpoint error.
                callback(make_error_code(error::general));
                return;
            }

            m_alog.write(log::alevel::devel,m_proxy_data->res.raw());

            if (m_proxy_data->res.get_status_code() != http::status_code::ok) {
                // got an error response back
                // TODO: expose this error in a programatically accessible way?
                // if so, see below for an option on how to do this.
                std::stringstream s;
                s << "Proxy connection error: " 
                  << m_proxy_data->res.get_status_code()
                  << " ("
                  << m_proxy_data->res.get_status_msg()
                  << ")";
                m_elog.write(log::elevel::info,s.str());
                callback(make_error_code(error::proxy_failed));
                return;
            }
            
            // we have successfully established a connection to the proxy, now
            // we can continue and the proxy will transparently forward the 
            // WebSocket connection.

            // TODO: decide if we want an on_proxy callback that would allow
            // access to the proxy response.
            
            // free the proxy buffers and req/res objects as they aren't needed
            // anymore
            m_proxy_data.reset();

            // call the original init handler back.
            callback(lib::error_code());
        }
    }
    
    /// read at least num_bytes bytes into buf and then call handler. 
    /**
     * 
     * 
     */
    void async_read_at_least(size_t num_bytes, char *buf, size_t len, 
        read_handler handler)
    {
        if (m_alog.static_test(log::alevel::devel)) {
            std::stringstream s;
            s << "asio async_read_at_least: " << num_bytes;
            m_alog.write(log::alevel::devel,s.str());
        }
        
        if (num_bytes > len) {
            m_elog.write(log::elevel::devel,
                "asio async_read_at_least error::invalid_num_bytes");
            handler(make_error_code(transport::error::invalid_num_bytes),
                size_t(0));
            return;
        }
        
        boost::asio::async_read(
            socket_con_type::get_socket(),
            boost::asio::buffer(buf,len),
            boost::asio::transfer_at_least(num_bytes),
            lib::bind(
                &type::handle_async_read,
                this,
                handler,
                lib::placeholders::_1,
                lib::placeholders::_2
            )
        );
    }
    
    void handle_async_read(read_handler handler, const 
        boost::system::error_code& ec, size_t bytes_transferred)
    {
        if (!ec) {
            handler(lib::error_code(), bytes_transferred);
            return;
        }
        
        // translate boost error codes into more lib::error_codes
        if (ec == boost::asio::error::eof) {
            handler(make_error_code(transport::error::eof), 
            bytes_transferred); 
        } else {
            // other error that we cannot translate into a WebSocket++ 
            // transport error. Use pass through and print an info warning
            // with the original error.
            std::stringstream s;
            s << "asio async_read_at_least error::pass_through"
              << ", Original Error: " << ec << " (" << ec.message() << ")";
            m_elog.write(log::elevel::info,s.str());
            handler(make_error_code(transport::error::pass_through), 
                bytes_transferred);
        }
    }
    
    void async_write(const char* buf, size_t len, write_handler handler) {
        m_bufs.push_back(boost::asio::buffer(buf,len));
        
        boost::asio::async_write(
            socket_con_type::get_socket(),
            m_bufs,
            lib::bind(
                &type::handle_async_write,
                this,
                handler,
                lib::placeholders::_1
            )
        );
    }
    
    void async_write(const std::vector<buffer>& bufs, write_handler handler) {
        std::vector<buffer>::const_iterator it;

        for (it = bufs.begin(); it != bufs.end(); ++it) {
            m_bufs.push_back(boost::asio::buffer((*it).buf,(*it).len));
        }
        
        boost::asio::async_write(
            socket_con_type::get_socket(),
            m_bufs,
            lib::bind(
                &type::handle_async_write,
                this,
                handler,
                lib::placeholders::_1
            )
        );
    }

    void handle_async_write(write_handler handler, const 
        boost::system::error_code& ec)
    {
        m_bufs.clear();
        // TODO: translate this better
        if (ec) {
            handler(make_error_code(error::pass_through));  
        } else {
            handler(lib::error_code());
        }
    }
    
    /// Set Connection Handle
    /**
     * See common/connection_hdl.hpp for information
     * 
     * @param hdl A connection_hdl that the transport will use to refer 
     * to itself
     */
    void set_handle(connection_hdl hdl) {
        m_connection_hdl = hdl;
    }
    
    /// Trigger the on_interrupt handler
    /**
     * This needs to be thread safe
     *
     * Might need a strand at some point?
     */
    lib::error_code interrupt(inturrupt_handler handler) {
        m_io_service->post(handler);
        return lib::error_code();
    }
    
    lib::error_code dispatch(dispatch_handler handler) {
        m_io_service->post(handler);
        return lib::error_code();
    }

    /*void handle_inturrupt(inturrupt_handler handler) { 
        handler();
    }*/
    
    /// close and clean up the underlying socket
    void shutdown() {
        socket_con_type::shutdown();
    }
    
    typedef lib::shared_ptr<boost::asio::deadline_timer> timer_ptr;
    
    timer_ptr set_timer(long duration, timer_handler handler) {
        timer_ptr timer(new boost::asio::deadline_timer(*m_io_service)); 
        timer->expires_from_now(boost::posix_time::milliseconds(duration));
        timer->async_wait(lib::bind(&type::timer_handler, this, handler, 
            lib::placeholders::_1));
        return timer;
    }
    
    void timer_handler(timer_handler h, const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted) {
            h(make_error_code(transport::error::operation_aborted));
        } else if (ec) {
            std::stringstream s;
            s << "asio async_wait error::pass_through"
              << "Original Error: " << ec << " (" << ec.message() << ")";
            m_elog.write(log::elevel::devel,s.str());
            h(make_error_code(transport::error::pass_through));
        } else {
            h(lib::error_code());
        }
    }
private:
    // static settings
    const bool          m_is_server;
    alog_type& m_alog;
    elog_type& m_elog;
    
    struct proxy_data {
        request_type req;
        response_type res;
        std::string write_buf;
        boost::asio::streambuf read_buf;
    };
    
    std::string m_proxy;
    lib::shared_ptr<proxy_data> m_proxy_data;
    
    // transport resources
    io_service_ptr      m_io_service;
    connection_hdl      m_connection_hdl;
    std::vector<boost::asio::const_buffer> m_bufs;

    // Handlers
    tcp_init_handler    m_tcp_init_handler;
};


} // namespace asio
} // namespace transport
} // namespace websocketpp

#endif // WEBSOCKETPP_TRANSPORT_ASIO_CON_HPP
