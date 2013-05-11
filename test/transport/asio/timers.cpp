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
//#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE transport_asio_timers
#include <boost/test/unit_test.hpp>

#include <exception>
#include <iostream>

#include <websocketpp/common/thread.hpp>

#include <websocketpp/transport/asio/endpoint.hpp>
#include <websocketpp/transport/asio/security/tls.hpp>

// Concurrency
#include <websocketpp/concurrency/none.hpp>

// HTTP
#include <websocketpp/http/request.hpp>
#include <websocketpp/http/response.hpp>

// Loggers
#include <websocketpp/logger/mock.hpp>

#include <boost/asio.hpp>

// Accept a connection, read data, and discard until EOF
void run_dummy_server() {
    using boost::asio::ip::tcp;
    
    try {    
        boost::asio::io_service io_service;
        tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v6(), 9005));
        tcp::socket socket(io_service);
    
        acceptor.accept(socket);
        for (;;) {
            char data[512];
            boost::system::error_code ec;
            socket.read_some(boost::asio::buffer(data), ec);
            if (ec == boost::asio::error::eof) {
                break;
            } else if (ec) {
                // other error
                throw ec;
            }
        }
    } catch (std::exception & e) {
        std::cout << e.what() << std::endl;
    } catch (boost::system::error_code & ec) {
        std::cout << ec.message() << std::endl;
    }
}

struct config {
    typedef websocketpp::concurrency::none concurrency_type;
    typedef websocketpp::log::mock alog_type;
    typedef websocketpp::log::mock elog_type;
    typedef websocketpp::http::parser::request request_type;
    typedef websocketpp::http::parser::response response_type;
    typedef websocketpp::transport::asio::tls_socket::endpoint socket_type;
    
    static const long timeout_socket_pre_init = 1000;
    static const long timeout_proxy = 1000;
    static const long timeout_socket_post_init = 1000;
    static const long timeout_dns_resolve = 1000;
    static const long timeout_connect = 1000;
    static const long timeout_socket_shutdown = 1000;
};

void run_test_timer() {
    boost::asio::io_service ios;
    boost::asio::deadline_timer t(ios,boost::posix_time::milliseconds(1000));
    t.wait();
    BOOST_FAIL( "Test timed out" );
}

typedef websocketpp::lib::shared_ptr<boost::asio::ssl::context> context_ptr;
context_ptr on_tls_init(websocketpp::connection_hdl hdl) {
        context_ptr ctx(new boost::asio::ssl::context(boost::asio::ssl::context::tlsv1));

        try {
            ctx->set_options(boost::asio::ssl::context::default_workarounds |
                             boost::asio::ssl::context::no_sslv2 |
                             boost::asio::ssl::context::single_dh_use);
        } catch (std::exception& e) {
            std::cout << e.what() << std::endl;
        }
        return ctx;
}

struct stub_con: public websocketpp::transport::asio::connection<config> {
    typedef websocketpp::transport::asio::connection<config> base;
    
    stub_con(bool a, config::alog_type& b, config::elog_type& c) : base(a,b,c) {}
    
    void start() {
        base::init(websocketpp::lib::bind(&stub_con::handle_start,this,
            websocketpp::lib::placeholders::_1));
    }
    
    void handle_start(const websocketpp::lib::error_code& ec) {
        using websocketpp::transport::asio::socket::make_error_code;
        using websocketpp::transport::asio::socket::error::tls_handshake_timeout;
        
        BOOST_CHECK_EQUAL( ec, make_error_code(tls_handshake_timeout) );
    }
};

typedef websocketpp::transport::asio::connection<config> con_type;
typedef websocketpp::lib::shared_ptr<stub_con> connection_ptr;

struct stub_endpoint : public websocketpp::transport::asio::endpoint<config> {
    typedef websocketpp::transport::asio::endpoint<config> base;
    
    stub_endpoint() {
        base::init_logging(&mock_logger,&mock_logger);
        init_asio();
    }
    
    connection_ptr connect(std::string u) {
        connection_ptr con(new stub_con(true,mock_logger,mock_logger));
        websocketpp::uri_ptr uri(new websocketpp::uri(u));
        
        
        
        BOOST_CHECK_EQUAL( base::init(con), websocketpp::lib::error_code() );
        
        base::async_connect(
            con,
            uri,
            websocketpp::lib::bind(
                &stub_endpoint::handle_connect,
                this,
                con,
                websocketpp::lib::placeholders::_1,
                websocketpp::lib::placeholders::_2
            )
        );
        
        return con;
    }
    
    void handle_connect(connection_ptr con, websocketpp::connection_hdl, 
        const websocketpp::lib::error_code & ec)
    {
        BOOST_CHECK( !ec );
        con->start();
    }

    void run() {
        base::run();
    }
    
    connection_ptr con;
    config::alog_type mock_logger;
};

BOOST_AUTO_TEST_CASE( tls_handshake_timeout ) {
    websocketpp::lib::thread dummy_server(&run_dummy_server);
    //websocketpp::lib::thread timer(&run_test_timer);
        
    stub_endpoint endpoint;
    endpoint.set_tls_init_handler(&on_tls_init);
    endpoint.connect("wss://localhost:9005");
    endpoint.run();
            
    BOOST_CHECK( true );
}