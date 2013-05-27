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
//#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE transport_integration
#include <boost/test/unit_test.hpp>

#include <websocketpp/common/thread.hpp>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/client.hpp>

typedef websocketpp::server<websocketpp::config::asio> server;
typedef websocketpp::client<websocketpp::config::asio_client> client;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

void run_server(server * s, int port) {
    try {
        s->clear_access_channels(websocketpp::log::alevel::all);
        s->clear_error_channels(websocketpp::log::elevel::all);

        s->init_asio();

        s->listen(port);
        s->start_accept();
        s->run();
    } catch (std::exception & e) {
        std::cout << e.what() << std::endl;
    } catch (boost::system::error_code & ec) {
        std::cout << ec.message() << std::endl;
    }
}

void run_client(client & c, std::string uri) {
    c.clear_access_channels(websocketpp::log::alevel::all);
    c.clear_error_channels(websocketpp::log::elevel::all);

    c.init_asio();
    
    websocketpp::lib::error_code ec;
    client::connection_ptr con = c.get_connection(uri,ec);
    BOOST_CHECK( !ec );
    c.connect(con);

    c.run();
}

bool on_ping(websocketpp::connection_hdl, std::string payload) {
    return false;
}

void cancel_on_open(server * s, websocketpp::connection_hdl hdl) {
    s->cancel();
}

template <typename T>
void ping_on_open(T * c, std::string payload, websocketpp::connection_hdl hdl) {
    typename T::connection_ptr con = c->get_con_from_hdl(hdl);
    con->ping(payload);
}

void fail_on_pong(websocketpp::connection_hdl hdl, std::string payload) {
    BOOST_FAIL( "expected no pong" );
}

template <typename T>
void req_pong_timeout(T * c, std::string expected_payload, 
    websocketpp::connection_hdl hdl, std::string payload)
{
    typename T::connection_ptr con = c->get_con_from_hdl(hdl);
    BOOST_CHECK_EQUAL( payload, expected_payload );
    con->close(websocketpp::close::status::normal,"");
}

// Wait for the specified time period then fail the test
void run_test_timer(long value) {
    /*boost::asio::io_service ios;
    boost::asio::deadline_timer t(ios,boost::posix_time::milliseconds(value));
    boost::system::error_code ec;
    t.wait(ec);*/
    sleep(value);
    BOOST_FAIL( "Test timed out" );
}

BOOST_AUTO_TEST_CASE( pong_timeout ) {
    server s;
    client c;

    s.set_ping_handler(on_ping);
    s.set_open_handler(bind(&cancel_on_open,&s,::_1));

    c.set_pong_handler(bind(&fail_on_pong,::_1,::_2));
    c.set_open_handler(bind(&ping_on_open<client>,&c,"foo",::_1));
    c.set_pong_timeout_handler(bind(&req_pong_timeout<client>,&c,"foo",::_1,::_2));

    websocketpp::lib::thread sthread(websocketpp::lib::bind(&run_server,&s,9005));
    websocketpp::lib::thread tthread(websocketpp::lib::bind(&run_test_timer,6));
    sthread.detach();
    tthread.detach();
    
    run_client(c, "http://localhost:9005");
}

