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

#ifndef WEBSOCKETPP_ROLE_SERVER_HPP
#define WEBSOCKETPP_ROLE_SERVER_HPP

#include "../processors/hybi.hpp"
#include "../processors/hybi_legacy.hpp"
#include "../rng/blank_rng.hpp"

#include "../shared_const_buffer.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/recursive_mutex.hpp>

#include <iostream>
#include <stdexcept>

#ifdef _MSC_VER
// Disable "warning C4355: 'this' : used in base member initializer list".
#   pragma warning(push)
#   pragma warning(disable:4355)
#endif

namespace websocketpp {

// Forward declarations
template <typename T> struct endpoint_traits;
    
namespace role {

template <class endpoint>
class server {
public:
    // Connection specific details
    template <typename connection_type>
    class connection : boost::noncopyable {
    public:
        typedef connection<connection_type> type;
        typedef endpoint endpoint_type;
        
        // Valid always
        int get_version() const {
            return m_version;
        }
        std::string get_request_header(const std::string& key) const {
            return m_request.header(key);
        }
        std::string get_origin() const {
            return m_origin;
        }
        
        // Information about the requested URI
        // valid only after URIs are loaded
        // TODO: check m_uri for NULLness
        bool get_secure() const {
            return m_uri->get_secure();
        }
        std::string get_host() const {
            return m_uri->get_host();
        }
        std::string get_resource() const {
            return m_uri->get_resource();
        }
        uint16_t get_port() const {
            return m_uri->get_port();
        }
        
        // Valid for CONNECTING state
        void add_response_header(const std::string& key, const std::string& value) {
            m_response.add_header(key,value);
        }
        void replace_response_header(const std::string& key, const std::string& value) {
            m_response.replace_header(key,value);
        }
        void remove_response_header(const std::string& key) {
            m_response.remove_header(key);
        }
        
        const std::vector<std::string>& get_subprotocols() const {
            return m_requested_subprotocols;
        }
        const std::vector<std::string>& get_extensions() const {
            return m_requested_extensions;
        }
        void select_subprotocol(const std::string& value);
        void select_extension(const std::string& value);
        
        // Valid if get_version() returns -1 (ie this is an http connection)
        void set_body(const std::string& value);
        
        int32_t rand() {
            return 0;
        }
        
        bool is_server() const {
            return true;
        }
        
        // should this exist?
        boost::asio::io_service& get_io_service() {
            return m_endpoint.get_io_service();
        }
    protected:
        connection(endpoint& e)
        : m_endpoint(e),
          m_connection(static_cast< connection_type& >(*this)),
          m_version(-1),
          m_uri() {}
        
        // initializes the websocket connection
        void async_init();
        void handle_read_request(const boost::system::error_code& error,
                                 std::size_t bytes_transferred);
        void handle_short_key3(const boost::system::error_code& error,
                                 std::size_t bytes_transferred);
        void write_response();
        void handle_write_response(const boost::system::error_code& error);
        
        void log_open_result();
    private:
        endpoint&                   m_endpoint;
        connection_type&            m_connection;
        
        int                         m_version;
        uri_ptr                     m_uri;
        std::string                 m_origin;
        std::vector<std::string>    m_requested_subprotocols;
        std::vector<std::string>    m_requested_extensions;
        std::string                 m_subprotocol;
        std::vector<std::string>    m_extensions;
        
        http::parser::request       m_request;
        http::parser::response      m_response;
        blank_rng                   m_rng;
    };
    
    // types
    typedef server<endpoint> type;
    typedef endpoint endpoint_type;
    
    typedef typename endpoint_traits<endpoint>::connection_ptr connection_ptr;
    typedef typename endpoint_traits<endpoint>::handler_ptr handler_ptr;
    
    // handler interface callback base class
    class handler_interface {
    public:
        virtual void on_handshake_init(connection_ptr con) {}
        
        virtual void validate(connection_ptr con) {}
        virtual void on_open(connection_ptr con) {}
        virtual void on_close(connection_ptr con) {}
        virtual void on_fail(connection_ptr con) {}
        
        virtual void on_message(connection_ptr con,message::data_ptr) {}
        
        virtual bool on_ping(connection_ptr con,std::string) {return true;}
        virtual void on_pong(connection_ptr con,std::string) {}
        virtual void on_pong_timeout(connection_ptr con,std::string) {}
        virtual void http(connection_ptr con) {}
    };
    
    server(boost::asio::io_service& m) 
     : m_endpoint(static_cast< endpoint_type& >(*this)),
       m_io_service(m),
       // the only way to set an endpoint address family appears to be using
       // this constructor, which also requires a port. This port number can be
       // ignored, as it is always overwriten later by the listen() member func
       m_acceptor(m),
       m_state(IDLE),
       m_timer(m,boost::posix_time::seconds(0)) {}
    
    void listen(uint16_t port, size_t n = 1);
    void listen(const boost::asio::ip::tcp::endpoint& e, size_t num_threads = 1);
    // uses internal resolver
    void listen(const std::string &host, const std::string &service, size_t n = 1);
    
    template <typename InternetProtocol> 
    void listen(const InternetProtocol &internet_protocol, uint16_t port, size_t n = 1) {
        m_endpoint.m_alog->at(log::alevel::DEVEL) 
            << "role::server listening on port " << port << log::endl;
        boost::asio::ip::tcp::endpoint e(internet_protocol, port);
        listen(e,n);
    }
protected:
    bool is_server() {
        return true;
    }
private:
    enum state {
        IDLE = 0,
        LISTENING = 1,
        STOPPING = 2,
        STOPPED = 3
    };
    
    // start_accept creates a new connection and begins an async_accept on it
    void start_accept();
    
    // handle_accept will begin the connection's read/write loop and then reset
    // the server to accept a new connection. Errors returned by async_accept
    // are logged and ingored.
    void handle_accept(connection_ptr con, 
                       const boost::system::error_code& error);
    
    endpoint_type&                  m_endpoint;
    boost::asio::io_service&        m_io_service;
    boost::asio::ip::tcp::acceptor  m_acceptor;
    state                           m_state;
    
    boost::asio::deadline_timer     m_timer;
};

template <class endpoint>
void server<endpoint>::listen(const boost::asio::ip::tcp::endpoint& e,size_t num_threads) {
    {
        boost::unique_lock<boost::recursive_mutex> lock(m_endpoint.m_lock);
        
        if (m_state != IDLE) {
            throw exception("listen called from invalid state.");
        }
        
        m_acceptor.open(e.protocol());
        m_acceptor.set_option(boost::asio::socket_base::reuse_address(true));
        m_acceptor.bind(e);
        m_acceptor.listen();
    
        this->start_accept();
    }
    
    if (num_threads == 1) {
        m_endpoint.run_internal();
    } else if (num_threads > 1 && num_threads <= MAX_THREAD_POOL_SIZE) {
        std::vector< boost::shared_ptr<boost::thread> > threads;
        
        for (std::size_t i = 0; i < num_threads; ++i) {
            boost::shared_ptr<boost::thread> thread(
                new boost::thread(boost::bind(
                        &endpoint_type::run_internal,
                        &m_endpoint
                ))
            );
            threads.push_back(thread);
        }
        
        for (std::size_t i = 0; i < threads.size(); ++i) {
            threads[i]->join();
        }
    } else {
        throw exception("listen called with invalid num_threads value");
    }
}

// server<endpoint> Implimentation
// TODO: provide a way to stop/reset the server endpoint
template <class endpoint>
void server<endpoint>::listen(uint16_t port, size_t n) {
    listen(boost::asio::ip::tcp::v6(), port, n);
}

template <class endpoint>
void server<endpoint>::listen(const std::string &host, const std::string &service, size_t n) {
    boost::asio::ip::tcp::resolver resolver(m_io_service);
    boost::asio::ip::tcp::resolver::query query(host, service);
    boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
    boost::asio::ip::tcp::resolver::iterator end;
    if (endpoint_iterator == end) {
        throw std::invalid_argument("Can't resolve host/service to listen");
    }
    const boost::asio::ip::tcp::endpoint &ep = *endpoint_iterator;
    listen(ep,n);
}

template <class endpoint>
void server<endpoint>::start_accept() {
    boost::lock_guard<boost::recursive_mutex> lock(m_endpoint.m_lock);
    
    connection_ptr con = m_endpoint.create_connection();
    
    if (con == connection_ptr()) {
        // the endpoint is no longer capable of accepting new connections.
        m_endpoint.m_alog->at(log::alevel::CONNECT) 
            << "Connection refused because endpoint is out of resources or closing." 
            << log::endl;
        return;
    }
        
    m_acceptor.async_accept(
        con->get_raw_socket(),
        boost::bind(
            &type::handle_accept,
            this,
            con,
            boost::asio::placeholders::error
        )
    );
}

// handle_accept will begin the connection's read/write loop and then reset
// the server to accept a new connection. Errors returned by async_accept
// are logged and ingored.
template <class endpoint>
void server<endpoint>::handle_accept(connection_ptr con, 
                                     const boost::system::error_code& error)
{
    boost::lock_guard<boost::recursive_mutex> lock(m_endpoint.m_lock);
    
    if (error) {
        con->m_fail_code = fail::status::SYSTEM;
        con->m_fail_system = error;
        
        if (error == boost::system::errc::too_many_files_open) {
            con->m_fail_reason = "too many files open";
            
            // TODO: make this configurable
            //m_timer.expires_from_now(boost::posix_time::milliseconds(1000));
            //m_timer.async_wait(boost::bind(&type::start_accept,this));
        } else if (error == boost::asio::error::operation_aborted) {
            con->m_fail_reason = "io_service operation canceled";
            
            // the operation was canceled. This was probably due to the 
            // io_service being stopped.
        } else {
            con->m_fail_reason = "unknown";
        }
        
        m_endpoint.m_elog->at(log::elevel::RERROR) 
                << "async_accept returned error: " << error 
                << " (" << con->m_fail_reason << ")" << log::endl;
        
        con->terminate(false);
    } else {
        con->start();
    }
    
    this->start_accept();
}
    
// server<endpoint>::connection<connnection_type> Implimentation

template <class endpoint>
template <class connection_type>
void server<endpoint>::connection<connection_type>::select_subprotocol(
                                                    const std::string& value)
{
    // TODO: should this be locked?
    
    std::vector<std::string>::iterator it;
    
    it = std::find(m_requested_subprotocols.begin(),
                   m_requested_subprotocols.end(),
                   value);
    
    if (value != "" && it == m_requested_subprotocols.end()) {
        throw std::invalid_argument("Attempted to choose a subprotocol not proposed by the client");
    }
    
    m_subprotocol = value;
}

template <class endpoint>
template <class connection_type>
void server<endpoint>::connection<connection_type>::select_extension(
                                                    const std::string& value)
{
    // TODO: should this be locked?
    
    if (value == "") {
        return;
    }
    
    std::vector<std::string>::iterator it;
    
    it = std::find(m_requested_extensions.begin(),
                   m_requested_extensions.end(),
                   value);
    
    if (it == m_requested_extensions.end()) {
        throw std::invalid_argument("Attempted to choose an extension not proposed by the client");
    }
    
    m_extensions.push_back(value);
}

// Valid if get_version() returns -1 (ie this is an http connection)
template <class endpoint>
template <class connection_type>
void server<endpoint>::connection<connection_type>::set_body(
                                                    const std::string& value)
{
    // TODO: should this be locked?
    
    if (m_connection.m_version != -1) {
        // TODO: throw exception
        throw std::invalid_argument("set_body called from invalid state");
    }
    
    m_response.set_body(value);
}
    
/// initiates an async read for an HTTP header
/**
 * Thread Safety: locks connection
 */
template <class endpoint>
template <class connection_type>
void server<endpoint>::connection<connection_type>::async_init() {
    boost::lock_guard<boost::recursive_mutex> lock(m_connection.m_lock);
    
    m_connection.get_handler()->on_handshake_init(m_connection.shared_from_this());
    
    // TODO: make this value configurable
    m_connection.register_timeout(5000,fail::status::TIMEOUT_WS,
                                       "Timeout on WebSocket handshake");
    
    boost::asio::async_read_until(
        m_connection.get_socket(),
        m_connection.buffer(),
        "\r\n\r\n",
        m_connection.get_strand().wrap(boost::bind(
            &type::handle_read_request,
            m_connection.shared_from_this(),
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred
        ))
    );
}

/// processes the response from an async read for an HTTP header
/**
 * Thread Safety: async asio calls are not thread safe
 */
template <class endpoint>
template <class connection_type>
void server<endpoint>::connection<connection_type>::handle_read_request(
    const boost::system::error_code& error, std::size_t /*bytes_transferred*/)
{
    if (error) {
        // log error
        m_endpoint.m_elog->at(log::elevel::RERROR) 
            << "Error reading HTTP request. code: " << error << log::endl;
        m_connection.terminate(false);
        return;
    }
    
    try {
        std::istream request(&m_connection.buffer());
        
        if (!m_request.parse_complete(request)) {
            // not a valid HTTP request/response
            throw http::exception("Received invalid HTTP Request",http::status_code::BAD_REQUEST);
        }
        
        // TODO: is there a way to short circuit this or something? 
        // This is often useful but slow to generate.
        //m_endpoint.m_alog.at(log::alevel::DEBUG_HANDSHAKE) << m_request.raw() << log::endl;
        
        std::string h = m_request.header("Upgrade");
        if (boost::ifind_first(h,"websocket")) {
            // Version is stored in the Sec-WebSocket-Version header for all 
            // versions after draft Hybi 00/Hixie 76. The absense of a version 
            // header is assumed to mean Hybi 00.
            h = m_request.header("Sec-WebSocket-Version");
            if (h == "") {
                m_version = 0;
            } else {
                m_version = atoi(h.c_str());
                if (m_version == 0) {
                    throw(http::exception("Unable to determine connection version",http::status_code::BAD_REQUEST));
                }
            }
            
            // Choose an appropriate websocket processor based on the version
            if (m_version == 0) {
                m_connection.m_processor = processor::ptr(
                    new processor::hybi_legacy<connection_type>(m_connection)
                );
                
                // Hybi legacy requires some extra out of band bookkeeping that 
                // future versions wont. We need to pull off an additional eight
                // bytes after the /r/n/r/n and store them somewhere that the
                // processor can find them.
                char foo[8];
                
                request.read(foo,8);
                
                if (request.gcount() != 8) {
                    size_t left = 8 - static_cast<size_t>(request.gcount());
                    // This likely occurs because the full key3 wasn't included
                    // in the asio read. It is likely that the extra bytes are
                    // actually on the wire and another asio read would get them
                    // Fixing this will require a way of restarting the 
                    // handshake read and storing the existing bytes until that
                    // comes back. Issue #101
                    
                    m_endpoint.m_elog->at(log::elevel::RERROR) 
                        << "Short Key 3: " << zsutil::to_hex(std::string(foo)) 
                        << " bytes missing: " << left 
                        << " eofbit: " << (request.eof() ? "true" : "false")
                        << " failbit: " << (request.fail() ? "true" : "false")
                        << " badbit: " << (request.bad() ? "true" : "false")
                        << " goodbit: " << (request.good() ? "true" : "false")
                        << log::endl;
                    
                    throw http::exception("Full Key3 not found in first chop",
                                          http::status_code::INTERNAL_SERVER_ERROR);
                    /*m_request.add_header("Sec-WebSocket-Key3",std::string(foo));
                    
                    
                    
                    boost::asio::async_read(
                        m_connection.get_socket(),
                        m_connection.buffer(),
                        boost::asio::transfer_at_least(left),
                        m_connection.get_strand().wrap(boost::bind(
                            &type::handle_short_key3,
                            m_connection.shared_from_this(),
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred
                        ))
                    );
                    return;*/
                }
                m_request.add_header("Sec-WebSocket-Key3",std::string(foo));
            } else if (m_version == 7 || m_version == 8 || m_version == 13) {
                m_connection.m_processor = processor::ptr(
                    new processor::hybi<connection_type>(m_connection)
                );
            } else {
                // version does not match any processor we have avaliable. Send 
                // an HTTP error and return the versions we do support in a the
                // appropriate response header.
                m_response.add_header("Sec-WebSocket-Version","13, 8, 7");
                
                throw(http::exception("Unsupported WebSocket version",http::status_code::BAD_REQUEST));
            }
            
            m_connection.m_processor->validate_handshake(m_request);
            m_origin = m_connection.m_processor->get_origin(m_request);
            m_uri = m_connection.m_processor->get_uri(m_request);
            
            m_endpoint.get_handler()->validate(m_connection.shared_from_this());
            
            m_response.set_status(http::status_code::SWITCHING_PROTOCOLS);
        } else {
            // continue as HTTP?
            m_endpoint.get_handler()->http(m_connection.shared_from_this());
            
            // should there be a more encapsulated http processor here?
            m_origin = m_request.header("Origin");
            
            // Set URI
            std::string h = m_request.header("Host");
            
            size_t last_colon = h.rfind(":");
            size_t last_sbrace = h.rfind("]");
            
            if (last_colon == std::string::npos || 
                (last_sbrace != std::string::npos && last_sbrace > last_colon))
            {
                // TODO: this makes the assumption that WS and HTTP
                // default ports are the same.
                m_uri.reset(new uri(m_endpoint.is_secure(),h,m_request.uri()));
            } else {
                m_uri.reset(new uri(m_endpoint.is_secure(),
                                    h.substr(0,last_colon),
                                    h.substr(last_colon+1),
                                    m_request.uri()));
            }
            
            m_response.set_status(http::status_code::OK);
        }
    } catch (const http::exception& e) {
        m_endpoint.m_elog->at(log::elevel::RERROR) << e.what() << log::endl;
        m_response.set_status(e.m_error_code,e.m_error_msg);
        m_response.set_body(e.m_body);
    } catch (const uri_exception& e) {
        // there was some error building the uri
        m_endpoint.m_elog->at(log::elevel::RERROR) << e.what() << log::endl;
        m_response.set_status(http::status_code::BAD_REQUEST);
    }
    
    write_response();
}

template <class endpoint>
template <class connection_type>
void server<endpoint>::connection<connection_type>::handle_short_key3 (
    const boost::system::error_code& error, std::size_t /*bytes_transferred*/)
{
    if (error) {
        // log error
        m_endpoint.m_elog->at(log::elevel::RERROR) 
            << "Error reading HTTP request. code: " << error << log::endl;
        m_connection.terminate(false);
        return;
    }
    
    try {
        std::istream request(&m_connection.buffer());
        
        std::string foo = m_request.header("Sec-WebSocket-Key3");
        
        size_t left = 8-foo.size();
        
        if (left == 0) {
            // ?????
            throw http::exception("handle_short_key3 called without short key",
                                  http::status_code::INTERNAL_SERVER_ERROR);
        }
        
        char foo2[9];
                
        request.get(foo2,left);
        
        if (request.gcount() != left) {
            // ?????
            throw http::exception("Full Key3 not found",
                                  http::status_code::INTERNAL_SERVER_ERROR);
        }
        
        m_endpoint.m_elog->at(log::elevel::RERROR) 
                        << "Recovered from Short Key 3: " << left << log::endl;
        
        foo.append(std::string(foo2));
        
        m_request.replace_header("Sec-WebSocket-Key3",foo2);
        
        m_connection.m_processor->validate_handshake(m_request);
        m_origin = m_connection.m_processor->get_origin(m_request);
        m_uri = m_connection.m_processor->get_uri(m_request);
        
        m_endpoint.get_handler()->validate(m_connection.shared_from_this());
        
        m_response.set_status(http::status_code::SWITCHING_PROTOCOLS);
    } catch (const http::exception& e) {
        m_endpoint.m_elog->at(log::elevel::RERROR) << e.what() << log::endl;
        m_response.set_status(e.m_error_code,e.m_error_msg);
        m_response.set_body(e.m_body);
    } catch (const uri_exception& e) {
        // there was some error building the uri
        m_endpoint.m_elog->at(log::elevel::RERROR) << e.what() << log::endl;
        m_response.set_status(http::status_code::BAD_REQUEST);
    }
    
    write_response();
}

template <class endpoint>
template <class connection_type>
void server<endpoint>::connection<connection_type>::write_response() {
    bool ws_response = true;
    
    m_response.set_version("HTTP/1.1");
    
    if (m_response.get_status_code() == http::status_code::SWITCHING_PROTOCOLS) {
        // websocket response
        m_connection.m_processor->handshake_response(m_request,m_response);
        
        if (m_subprotocol != "") {
            m_response.replace_header("Sec-WebSocket-Protocol",m_subprotocol);
        }
        
        // TODO: return negotiated extensions
    } else {
        // TODO: HTTP response
        ws_response = false;
    }
    
    m_response.replace_header("Server",USER_AGENT);
    
    std::string raw = m_response.raw();
    
    // Hack for legacy HyBi
    if (ws_response && m_version == 0) {
        raw += boost::dynamic_pointer_cast<processor::hybi_legacy<connection_type> >(m_connection.m_processor)->get_key3();
    }
    
    shared_const_buffer buffer(raw);
    
    m_endpoint.m_alog->at(log::alevel::DEBUG_HANDSHAKE) << raw << log::endl;
    
    boost::asio::async_write(
        m_connection.get_socket(),
        //boost::asio::buffer(raw),
        buffer,
        boost::bind(
            &type::handle_write_response,
            m_connection.shared_from_this(),
            boost::asio::placeholders::error
        )
    );
}

template <class endpoint>
template <class connection_type>
void server<endpoint>::connection<connection_type>::handle_write_response(
    const boost::system::error_code& error)
{
    if (error) {
        m_endpoint.m_elog->at(log::elevel::RERROR) 
            << "Network error writing handshake respons. code: " << error 
            << log::endl;
        
        m_connection.terminate(false);
        return;
    }
    
    if (m_response.get_status_code() != http::status_code::SWITCHING_PROTOCOLS) {
        if (m_version == -1) {
            // if this was not a websocket connection, we have written 
            // the expected response and the connection can be closed.
        } else {
            // this was a websocket connection that ended in an error
            m_endpoint.m_elog->at(log::elevel::RERROR) 
                << "Handshake ended with HTTP error: " 
                << m_response.get_status_code() << " " 
                << m_response.get_status_msg() << log::endl;
        }
        m_connection.terminate(true);
        return;
    }
    
    m_connection.cancel_timeout();
    
    log_open_result();
    
    m_connection.m_state = session::state::OPEN;
    
    m_connection.get_handler()->on_open(m_connection.shared_from_this());
    
    get_io_service().post(
        m_connection.m_strand.wrap(boost::bind(
            &connection_type::handle_read_frame,
            m_connection.shared_from_this(),
            boost::system::error_code()
        ))
    );
    
    //m_connection.handle_read_frame(boost::system::error_code());
}

template <class endpoint>
template <class connection_type>
void server<endpoint>::connection<connection_type>::log_open_result() {
    std::stringstream version;
    version << "v" << m_version << " ";
    
    std::string remote;
    boost::system::error_code ec;
    boost::asio::ip::tcp::endpoint ep = m_connection.get_raw_socket().remote_endpoint(ec);
    if (ec) {
        m_endpoint.m_elog->at(log::elevel::WARN) 
            << "Error getting remote endpoint. code: " << ec << log::endl;
    }
    
    m_endpoint.m_alog->at(log::alevel::CONNECT) 
        << (m_version == -1 ? "HTTP" : "WebSocket") << " Connection ";
    
    if (ec) {
        m_endpoint.m_alog->at(log::alevel::CONNECT) << "Unknown";
    } else {
        m_endpoint.m_alog->at(log::alevel::CONNECT) << ep;
    }
    
    m_endpoint.m_alog->at(log::alevel::CONNECT) << " "
        << (m_version == -1 ? "" : version.str())
        << (get_request_header("User-Agent") == "" ? "NULL" : get_request_header("User-Agent")) 
        << " " << (m_uri ? m_uri->get_resource() : "uri is NULL") << " " 
        << m_response.get_status_code() << log::endl;
}
    
} // namespace role 
} // namespace websocketpp

#ifdef _MSC_VER
#   pragma warning(pop)
#endif

#endif // WEBSOCKETPP_ROLE_SERVER_HPP
