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

#ifndef WEBSOCKETPP_ROLE_CLIENT_HPP
#define WEBSOCKETPP_ROLE_CLIENT_HPP

#include <limits>
#include <iostream>

#include <boost/cstdint.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/random.hpp>
#include <boost/random/random_device.hpp>

#include "../endpoint.hpp"
#include "../uri.hpp"
#include "../shared_const_buffer.hpp"

#ifdef _MSC_VER
// Disable "warning C4355: 'this' : used in base member initializer list".
#   pragma warning(push)
#   pragma warning(disable:4355)
#endif

using boost::asio::ip::tcp;

namespace websocketpp {
namespace role {
    
template <class endpoint>
class client {
public:
    // Connection specific details
    template <typename connection_type>
    class connection {
    public:
        typedef connection<connection_type> type;
        typedef endpoint endpoint_type;
        
        // client connections are friends with their respective client endpoint
        friend class client<endpoint>;
        
        // Valid always
        int get_version() const {
            return m_version;
        }
        
        std::string get_origin() const {
            return m_origin;
        }
        
        // not sure when valid
        std::string get_request_header(const std::string& key) const {
            return m_request.header(key);
        }
        std::string get_response_header(const std::string& key) const {
            return m_response.header(key);
        }
        
        // Valid before connect is called
        void add_request_header(const std::string& key, const std::string& value) {
            m_request.add_header(key,value);
        }
        void replace_request_header(const std::string& key, const std::string& value) {
            m_request.replace_header(key,value);
        }
        void remove_request_header(const std::string& key) {
            m_request.remove_header(key);
        }
        
        void add_subprotocol(const std::string& value) {
            m_requested_subprotocols.push_back(value);
        }
        
        void set_origin(const std::string& value) {
            m_origin = value;
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
        std::string get_uri() const {
            return m_uri->str();
        }
        
        int32_t rand() {
            return m_endpoint.rand();
        }
        
        bool is_server() const {
            return false;
        }
        
        // should this exist?
        boost::asio::io_service& get_io_service() {
            return m_endpoint.get_io_service();
        }
    protected:
        connection(endpoint& e) 
         : m_endpoint(e),
           m_connection(static_cast< connection_type& >(*this)),
           // TODO: version shouldn't be hardcoded
           m_version(13) {}
        
        void set_uri(uri_ptr u) {
            m_uri = u;
        }
        
        void async_init() {
            m_connection.m_processor = processor::ptr(new processor::hybi<connection_type>(m_connection));
            
            write_request();
        }
        
        void write_request();
        void handle_write_request(const boost::system::error_code& error);
        void read_response();
        void handle_read_response(const boost::system::error_code& error,
                                  std::size_t bytes_transferred);
        
        void log_open_result();
    private:
        endpoint&           m_endpoint;
        connection_type&    m_connection;
        
        int                         m_version;
        uri_ptr                     m_uri;
        std::string                 m_origin;
        std::vector<std::string>    m_requested_subprotocols;
        std::vector<std::string>    m_requested_extensions;
        std::string                 m_subprotocol;
        std::vector<std::string>    m_extensions;
        
        std::string                 m_handshake_key;
        http::parser::request       m_request;
        http::parser::response      m_response;
    };
    
    // types
    typedef client<endpoint> type;
    typedef endpoint endpoint_type;
    
    typedef typename endpoint_traits<endpoint>::connection_type connection_type;
    typedef typename endpoint_traits<endpoint>::connection_ptr connection_ptr;
    typedef typename endpoint_traits<endpoint>::handler_ptr handler_ptr;
    
    // handler interface callback class
    class handler_interface {
    public:
        // Required
        virtual void on_open(connection_ptr con) {}
        virtual void on_close(connection_ptr con) {}
        virtual void on_fail(connection_ptr con) {}
        
        virtual void on_message(connection_ptr con,message::data_ptr) {}
        
        // Optional
        virtual bool on_ping(connection_ptr con,std::string) {return true;}
        virtual void on_pong(connection_ptr con,std::string) {}
        virtual void on_pong_timeout(connection_ptr con,std::string) {}
        
    };
    
    client (boost::asio::io_service& m) 
     : m_endpoint(static_cast< endpoint_type& >(*this)),
       m_io_service(m),
       m_gen(m_rng,
             boost::random::uniform_int_distribution<>(
                std::numeric_limits<int32_t>::min(),
                std::numeric_limits<int32_t>::max()
             )) {}
    
    connection_ptr get_connection(const std::string& u);
    
    connection_ptr connect(const std::string& u);
    connection_ptr connect(connection_ptr con);
    
    void run(bool perpetual = false);
    void end_perpetual();
    void reset();
protected:
    bool is_server() const {return false;}
    int32_t rand() {return m_gen();}
private:
    void handle_connect(connection_ptr con, const boost::system::error_code& error);
    
    endpoint_type&              m_endpoint;
    boost::asio::io_service&    m_io_service;
    
    boost::random::random_device    m_rng;
    boost::random::variate_generator<
        boost::random::random_device&,
        boost::random::uniform_int_distribution<>
    > m_gen;
    
    boost::shared_ptr<boost::asio::io_service::work> m_idle_worker;
};

// client implimentation

/// Start the client ASIO loop
/**
 * Calls run on the endpoint's io_service. This method will block until the io_service
 * run method returns. This method may only be called when the endpoint is in the IDLE
 * state. Endpoints start in the idle state and can be returned to the IDLE state by
 * calling reset. `run` has a perpetual flag (default is false) that indicates whether
 * or not it should return after all connections have been made.
 *
 * <b>Important note:</b> Calling run with perpetual = false on a client endpoint will return
 * immediately unless you have already called connect() at least once. To get around
 * this either queue up all connections you want to make before calling run or call
 * run with perpetual in another thread.
 * 
 * Visibility: public
 * State: Valid from IDLE, an exception is thrown otherwise
 * Concurrency: callable from any thread
 *
 * @param perpetual whether or not to run the endpoint in perpetual mode
 * @exception websocketpp::exception with code error::INVALID_STATE if called from a state other than IDLE
 */
template <class endpoint>
void client<endpoint>::run(bool perpetual) {
    {
        boost::lock_guard<boost::recursive_mutex> lock(m_endpoint.m_lock);
        
        if (m_endpoint.m_state != endpoint::IDLE) {
            throw exception("client::run called from invalid state",error::INVALID_STATE);
        }
        
        if (perpetual) {
            m_idle_worker = boost::shared_ptr<boost::asio::io_service::work>(
                new boost::asio::io_service::work(m_io_service)
            );
        }
        
        m_endpoint.m_state = endpoint::RUNNING;
    }
    
    // TODO: preliminary support for multi-threaded clients. Finish external
    // interface once better tested
    size_t num_threads = 1;
    
    if (num_threads == 1) {
        m_io_service.run();
    } else if (num_threads > 1 && num_threads <= MAX_THREAD_POOL_SIZE) {
        std::vector< boost::shared_ptr<boost::thread> > threads;
        
        for (std::size_t i = 0; i < num_threads; ++i) {
            boost::shared_ptr<boost::thread> thread(
                new boost::thread(boost::bind(
                        &boost::asio::io_service::run,
                        &m_io_service
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
    
    m_endpoint.m_state = endpoint::STOPPED;
}

/// End the idle work loop that keeps the io_service active
/**
 * Calling end_perpetual on a client endpoint that was started in perpetual mode (via
 * run(true), will stop the idle work object that prevents the run method from 
 * returning even when there is no work for it to do. Use if you want to gracefully
 * stop the endpoint. Use stop() to forcibly stop the endpoint.
 * 
 * Visibility: public
 * State: Valid from RUNNING, ignored otherwise
 * Concurrency: callable from any thread
 */
template <class endpoint>
void client<endpoint>::end_perpetual() {
    if (m_idle_worker) {
        m_idle_worker.reset();
    }
}

/// Reset a stopped endpoint.
/**
 * Resets an endpoint that was stopped by stop() or whose run() method exited due to
 * running out of work. reset() should not be called while the endpoint is running.
 * Use stop() and/or end_perpetual() first and then reset once one of those methods
 * has fully stopped the endpoint.
 * 
 * Visibility: public
 * State: Valid from STOPPED, an exception is thrown otherwise
 * Concurrency: callable from any thread
 */
template <class endpoint>
void client<endpoint>::reset() {
    boost::lock_guard<boost::recursive_mutex> lock(m_endpoint.m_lock);
        
    if (m_endpoint.m_state != endpoint::STOPPED) {
        throw exception("client::reset called from invalid state",error::INVALID_STATE);
    }
    
    m_io_service.reset();
    
    m_endpoint.m_state = endpoint::IDLE;
}

/// Returns a new connection 
/**
 * Creates and returns a pointer to a new connection to the given URI suitable for passing
 * to connect(). This method allows applying connection specific settings before 
 * performing the connection.
 * 
 * Visibility: public
 * State: Valid from IDLE or RUNNING, an exception is thrown otherwise
 * Concurrency: callable from any thread
 *
 * @param u The URI that this connection will connect to.
 * @return The pointer to the new connection
 */
template <class endpoint>
typename endpoint_traits<endpoint>::connection_ptr
client<endpoint>::get_connection(const std::string& u) {
    try {
        uri_ptr location(new uri(u));
        
        if (location->get_secure() && !m_endpoint.is_secure()) {
            throw websocketpp::exception("Endpoint doesn't support secure connections.",
                websocketpp::error::ENDPOINT_UNSECURE);
        }
        
        connection_ptr con = m_endpoint.create_connection();
        
        if (!con) {
            throw websocketpp::exception("get_connection called from invalid state",
                websocketpp::error::INVALID_STATE);
        }
        
        con->set_uri(location);
        
        return con;
    } catch (uri_exception& e) {
        throw websocketpp::exception(e.what(),websocketpp::error::INVALID_URI);
    }
}

/// Begin the connect process for the given connection.
/**
 * Initiates the async connect request for connection con.
 * 
 * Visibility: public
 * State: Valid from IDLE or RUNNING, an exception is thrown otherwise
 * Concurrency: callable from any thread
 *
 * @param con A pointer to the connection to connect
 * @return The pointer to con
 */
template <class endpoint>
typename endpoint_traits<endpoint>::connection_ptr
client<endpoint>::connect(connection_ptr con) {
    tcp::resolver resolver(m_io_service);
    
    std::stringstream p;
    p << con->get_port();
    
    tcp::resolver::query query(con->get_host(),p.str());
    tcp::resolver::iterator iterator = resolver.resolve(query);
    
    boost::asio::async_connect(
        con->get_raw_socket(),
        iterator,
        boost::bind(
            &endpoint_type::handle_connect,
            this, // shared from this?
            con,
            boost::asio::placeholders::error
        )
    ); 
    
    return con;
}

/// Convenience method, equivalent to connect(get_connection(u))
template <class endpoint>
typename endpoint_traits<endpoint>::connection_ptr
client<endpoint>::connect(const std::string& u) {
    return connect(get_connection(u));
}

template <class endpoint>
void client<endpoint>::handle_connect(connection_ptr con, 
                                      const boost::system::error_code& error)
{
    if (!error) {
        m_endpoint.m_alog->at(log::alevel::CONNECT)
            << "Successful connection" << log::endl;
        
        con->start();
    } else {
        con->m_fail_code = fail::status::SYSTEM;
        con->m_fail_system = error;
        
        if (error == boost::system::errc::connection_refused) {
            con->m_fail_reason = "Connection refused";
        } else if (error == boost::system::errc::operation_canceled) {
            con->m_fail_reason = "Operation canceled";
        } else if (error == boost::system::errc::connection_reset) {
            con->m_fail_reason = "Connection Reset";
        } else if (error == boost::system::errc::timed_out) {
            con->m_fail_reason = "Operation timed out";
        } else if (error == boost::system::errc::broken_pipe) {
            con->m_fail_reason = "Broken pipe";
        } else {
            con->m_fail_reason = "Unknown";
        }
        
        m_endpoint.m_elog->at(log::elevel::RERROR) 
                << "An error occurred while establishing a connection: " 
                << error << " (" << con->m_fail_reason << ")" << log::endl;
        
        con->terminate(false);
    }
}

// client connection implimentation

template <class endpoint>
template <class connection_type>
void client<endpoint>::connection<connection_type>::write_request() {
    boost::lock_guard<boost::recursive_mutex> lock(m_connection.m_lock);
    // async write to handle_write
    
    m_request.set_method("GET");
    m_request.set_uri(m_uri->get_resource());
    m_request.set_version("HTTP/1.1");
    
    m_request.add_header("Upgrade","websocket");
    m_request.add_header("Connection","Upgrade");
    m_request.replace_header("Sec-WebSocket-Version","13");
    
    m_request.replace_header("Host",m_uri->get_host_port());
    
    if (m_origin != "") {
         m_request.replace_header("Origin",m_origin);
    }
    
    if (m_requested_subprotocols.size() > 0) {
        std::string vals;
        std::string sep = "";
        
        std::vector<std::string>::iterator it;
        for (it = m_requested_subprotocols.begin(); it != m_requested_subprotocols.end(); ++it) {
            vals += sep + *it;
            sep = ",";
        }
        
        m_request.replace_header("Sec-WebSocket-Protocol",vals);
    }
    
    // Generate client key
    int32_t raw_key[4];
    
    for (int i = 0; i < 4; i++) {
        raw_key[i] = this->rand();
    }
    
    m_handshake_key = base64_encode(reinterpret_cast<unsigned char const*>(raw_key), 16);
    
    m_request.replace_header("Sec-WebSocket-Key",m_handshake_key);
    
    // Unless the user has overridden the user agent, send generic WS++
    if (m_request.header("User-Agent") == "") {
        m_request.replace_header("User-Agent",USER_AGENT);
    }
    
    
    
    // TODO: generating this raw request involves way too much copying in cases
    //       without string/vector move semantics.
    shared_const_buffer buffer(m_request.raw());
    
    //std::string raw = m_request.raw();
    
    //m_endpoint.m_alog->at(log::alevel::DEBUG_HANDSHAKE) << raw << log::endl;
    
    boost::asio::async_write(
        m_connection.get_socket(),
        //boost::asio::buffer(raw),
        buffer,
        m_connection.get_strand().wrap(boost::bind(
            &type::handle_write_request,
            m_connection.shared_from_this(),
            boost::asio::placeholders::error
        ))
    );
}

template <class endpoint>
template <class connection_type>
void client<endpoint>::connection<connection_type>::handle_write_request(
    const boost::system::error_code& error)
{
    if (error) {
        // TODO: detached state?
        
        m_endpoint.m_elog->at(log::elevel::RERROR) 
            << "Error writing WebSocket request. code: " 
            << error << log::endl;
        m_connection.terminate(false);
        return;
    }
    
    read_response();
}

template <class endpoint>
template <class connection_type>
void client<endpoint>::connection<connection_type>::read_response() {
    boost::asio::async_read_until(
        m_connection.get_socket(),
        m_connection.buffer(),
            "\r\n\r\n",
        m_connection.get_strand().wrap(boost::bind(
            &type::handle_read_response,
            m_connection.shared_from_this(),
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred
        ))
    );
}

template <class endpoint>
template <class connection_type>
void client<endpoint>::connection<connection_type>::handle_read_response (
        const boost::system::error_code& error, std::size_t bytes_transferred)
{
    boost::lock_guard<boost::recursive_mutex> lock(m_connection.m_lock);
    
    // detached check?
    
    if (error) {
        m_endpoint.m_elog->at(log::elevel::RERROR)
            << "Error reading HTTP request. code: " << error << log::endl;
        m_connection.terminate(false);
        return;
    }
    
    try {
        std::istream request(&m_connection.buffer());
        
        if (!m_response.parse_complete(request)) {
            // not a valid HTTP response
            // TODO: this should be a client error
            throw http::exception("Could not parse server response.",
                                  http::status_code::BAD_REQUEST);
        }
        
        m_endpoint.m_alog->at(log::alevel::DEBUG_HANDSHAKE) << m_response.raw() 
                                                           << log::endl;
        
        // error checking
        if (m_response.get_status_code() != http::status_code::SWITCHING_PROTOCOLS) {
            throw http::exception("Server failed to upgrade connection.",
                                  m_response.get_status_code(),
                                  m_response.get_status_msg());
        }
        
        std::string h = m_response.header("Upgrade");
        if (!boost::ifind_first(h,"websocket")) {
            throw http::exception("Token `websocket` missing from Upgrade header.",
                                  m_response.get_status_code(),
                                  m_response.get_status_msg());
        }
        
        h = m_response.header("Connection");
        if (!boost::ifind_first(h,"upgrade")) {
            throw http::exception("Token `upgrade` missing from Connection header.",
                                  m_response.get_status_code(),
                                  m_response.get_status_msg());
        }
        
        h = m_response.header("Sec-WebSocket-Accept");
        if (h == "") {
            throw http::exception("Required Sec-WebSocket-Accept header is missing.",
                                  m_response.get_status_code(),
                                  m_response.get_status_msg());
        } else {
            std::string server_key = m_handshake_key;
            server_key += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
            
            SHA1        sha;
            uint32_t    message_digest[5];
            
            sha.Reset();
            sha << server_key.c_str();
            
            if (!sha.Result(message_digest)) {
                m_endpoint.m_elog->at(log::elevel::RERROR)
                    << "Error computing handshake sha1 hash." << log::endl;
                // TODO: close behavior
                return;
            }
            
            // convert sha1 hash bytes to network byte order because this sha1
            //  library works on ints rather than bytes
            for (int i = 0; i < 5; i++) {
                message_digest[i] = htonl(message_digest[i]);
            }
            
            server_key = base64_encode(
                reinterpret_cast<const unsigned char*>(message_digest),20);
            if (server_key != h) {
                m_endpoint.m_elog->at(log::elevel::RERROR)
                    << "Server returned incorrect handshake key." << log::endl;
                // TODO: close behavior
                return;
            }
        }
        
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
    } catch (const http::exception& e) {
        m_endpoint.m_elog->at(log::elevel::RERROR) 
            << "Error processing server handshake. Server HTTP response: " 
            << e.m_error_msg << " (" << e.m_error_code 
            << ") Local error: " << e.what() << log::endl;
        return;
    }
    
    
    // start session loop
}

template <class endpoint>
template <class connection_type>
void client<endpoint>::connection<connection_type>::log_open_result() {
    std::stringstream version;
    version << "v" << m_version << " ";
        
    m_endpoint.m_alog->at(log::alevel::CONNECT)
        << (m_version == -1 ? "HTTP" : "WebSocket") << " Connection "
        << m_connection.get_raw_socket().remote_endpoint() << " "
        << (m_version == -1 ? "" : version.str())
        << (get_request_header("Server") == "" ? "NULL" : get_request_header("Server")) 
        << " " << m_uri->get_resource() << " " << m_response.get_status_code() 
        << log::endl;
}
    
} // namespace role
} // namespace websocketpp

#ifdef _MSC_VER
#   pragma warning(pop)
#endif

#endif // WEBSOCKETPP_ROLE_CLIENT_HPP
