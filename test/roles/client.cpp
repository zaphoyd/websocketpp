/*
 * Copyright (c) 2014, Peter Thorson. All rights reserved.
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
#define BOOST_TEST_MODULE client
#include <boost/test/unit_test.hpp>

#include <iostream>

#include <websocketpp/random/random_device.hpp>

#include <websocketpp/config/core.hpp>
#include <websocketpp/client.hpp>

#include <websocketpp/http/request.hpp>
#include <websocketpp/http/response.hpp>
#include <websocketpp/processors/hybi13.hpp>

struct stub_config : public websocketpp::config::core {
    typedef core::concurrency_type concurrency_type;

    typedef core::request_type request_type;
    typedef core::response_type response_type;

    typedef core::message_type message_type;
    typedef core::con_msg_manager_type con_msg_manager_type;
    typedef core::endpoint_msg_manager_type endpoint_msg_manager_type;

    typedef core::alog_type alog_type;
    typedef core::elog_type elog_type;

    //typedef core::rng_type rng_type;
    typedef websocketpp::random::random_device::int_generator<uint32_t,concurrency_type> rng_type;

    typedef core::transport_type transport_type;

    typedef core::endpoint_base endpoint_base;

    static const websocketpp::log::level elog_level = websocketpp::log::elevel::none;
    static const websocketpp::log::level alog_level = websocketpp::log::alevel::none;
};

typedef websocketpp::client<stub_config> client;
typedef client::connection_ptr connection_ptr;

BOOST_AUTO_TEST_CASE( invalid_uri ) {
    client c;
    websocketpp::lib::error_code ec;

    connection_ptr con = c.get_connection("foo", ec);

    BOOST_CHECK_EQUAL( ec , websocketpp::error::make_error_code(websocketpp::error::invalid_uri) );
}

BOOST_AUTO_TEST_CASE( unsecure_endpoint ) {
    client c;
    websocketpp::lib::error_code ec;

    connection_ptr con = c.get_connection("wss://localhost/", ec);

    BOOST_CHECK_EQUAL( ec , websocketpp::error::make_error_code(websocketpp::error::endpoint_not_secure) );
}

BOOST_AUTO_TEST_CASE( get_connection ) {
    client c;
    websocketpp::lib::error_code ec;

    connection_ptr con = c.get_connection("ws://localhost/", ec);

    BOOST_CHECK( con );
    BOOST_CHECK_EQUAL( con->get_host() , "localhost" );
    BOOST_CHECK_EQUAL( con->get_port() , 80 );
    BOOST_CHECK_EQUAL( con->get_secure() , false );
    BOOST_CHECK_EQUAL( con->get_resource() , "/" );
}

BOOST_AUTO_TEST_CASE( connect_con ) {
    client c;
    websocketpp::lib::error_code ec;
    std::stringstream out;
    std::string o;

    c.register_ostream(&out);

    connection_ptr con = c.get_connection("ws://localhost/", ec);
    BOOST_CHECK_EQUAL(ec, websocketpp::lib::error_code());
    c.connect(con);

    o = out.str();
    websocketpp::http::parser::request r;
    r.consume(o.data(),o.size(),ec);
    BOOST_CHECK_EQUAL(ec, websocketpp::lib::error_code());

    BOOST_CHECK( r.ready() );
    BOOST_CHECK_EQUAL( r.get_method(), "GET");
    BOOST_CHECK_EQUAL( r.get_version(), "HTTP/1.1");
    BOOST_CHECK_EQUAL( r.get_uri(), "/");

    BOOST_CHECK_EQUAL( r.get_header("Host"), "localhost");
    BOOST_CHECK_EQUAL( r.get_header("Sec-WebSocket-Version"), "13");
    BOOST_CHECK_EQUAL( r.get_header("Connection"), "Upgrade");
    BOOST_CHECK_EQUAL( r.get_header("Upgrade"), "websocket");

    // Key is randomly generated & User-Agent will change so just check that
    // they are not empty.
    BOOST_CHECK_NE( r.get_header("Sec-WebSocket-Key"), "");
    BOOST_CHECK_NE( r.get_header("User-Agent"), "" );

    // connection should have written out an opening handshake request and be in
    // the read response internal state

    // TODO: more tests related to reading the HTTP response
    std::stringstream channel2;
    channel2 << "e\r\n\r\n";
    channel2 >> *con;
}

BOOST_AUTO_TEST_CASE( select_subprotocol ) {
    client c;
    websocketpp::lib::error_code ec;
    using websocketpp::error::make_error_code;

    connection_ptr con = c.get_connection("ws://localhost/", ec);

    BOOST_CHECK( con );

    con->select_subprotocol("foo",ec);
    BOOST_CHECK_EQUAL( ec , make_error_code(websocketpp::error::server_only) );
    BOOST_CHECK_THROW( con->select_subprotocol("foo") , websocketpp::exception );
}

BOOST_AUTO_TEST_CASE( add_subprotocols_invalid ) {
    client c;
    websocketpp::lib::error_code ec;
    using websocketpp::error::make_error_code;

    connection_ptr con = c.get_connection("ws://localhost/", ec);
    BOOST_CHECK( con );

    con->add_subprotocol("",ec);
    BOOST_CHECK_EQUAL( ec , make_error_code(websocketpp::error::invalid_subprotocol) );
    BOOST_CHECK_THROW( con->add_subprotocol("") , websocketpp::exception );

    con->add_subprotocol("foo,bar",ec);
    BOOST_CHECK_EQUAL( ec , make_error_code(websocketpp::error::invalid_subprotocol) );
    BOOST_CHECK_THROW( con->add_subprotocol("foo,bar") , websocketpp::exception );
}

BOOST_AUTO_TEST_CASE( add_subprotocols ) {
    client c;
    websocketpp::lib::error_code ec;
    std::stringstream out;
    std::string o;

    c.register_ostream(&out);

    connection_ptr con = c.get_connection("ws://localhost/", ec);
    BOOST_CHECK_EQUAL(ec, websocketpp::lib::error_code());
    BOOST_CHECK( con );

    con->add_subprotocol("foo",ec);
    BOOST_CHECK_EQUAL(ec, websocketpp::lib::error_code());

    BOOST_CHECK_NO_THROW( con->add_subprotocol("bar") );

    c.connect(con);

    o = out.str();
    websocketpp::http::parser::request r;
    r.consume(o.data(),o.size(),ec);
    BOOST_CHECK_EQUAL(ec, websocketpp::lib::error_code());

    BOOST_CHECK( r.ready() );
    BOOST_CHECK_EQUAL( r.get_header("Sec-WebSocket-Protocol"), "foo, bar");
}

// Reproduces issue #570: a client that offers subprotocols and receives a
// valid server handshake selecting one should expose the negotiated value
// via get_subprotocol().
BOOST_AUTO_TEST_CASE( negotiated_subprotocol ) {
    client c;
    websocketpp::lib::error_code ec;
    std::stringstream out;

    c.register_ostream(&out);

    connection_ptr con = c.get_connection("ws://localhost/", ec);
    BOOST_CHECK_EQUAL(ec, websocketpp::lib::error_code());
    BOOST_CHECK( con );

    con->add_subprotocol("foo",ec);
    BOOST_CHECK_EQUAL(ec, websocketpp::lib::error_code());
    con->add_subprotocol("bar",ec);
    BOOST_CHECK_EQUAL(ec, websocketpp::lib::error_code());

    c.connect(con);

    // Parse the opening handshake request the client wrote out so we can build
    // a matching response.
    std::string o = out.str();
    websocketpp::http::parser::request r;
    r.consume(o.data(),o.size(),ec);
    BOOST_CHECK_EQUAL(ec, websocketpp::lib::error_code());
    BOOST_CHECK( r.ready() );

    // Build a valid server handshake response selecting "foo". Using the
    // hybi13 processor ensures Sec-WebSocket-Accept matches the randomly
    // generated client key, so the client accepts the response as valid.
    stub_config::con_msg_manager_type::ptr mm(
        websocketpp::lib::make_shared<stub_config::con_msg_manager_type>());
    stub_config::rng_type rng;
    websocketpp::processor::hybi13<stub_config> proc(false,true,mm,rng);

    websocketpp::http::parser::response res;
    res.set_version("HTTP/1.1");
    res.set_status(websocketpp::http::status_code::switching_protocols);
    ec = proc.process_handshake(r,"foo",res);
    BOOST_CHECK_EQUAL(ec, websocketpp::lib::error_code());
    BOOST_CHECK_EQUAL( res.get_header("Sec-WebSocket-Protocol"), "foo" );

    // Feed the server's response back into the client connection.
    std::stringstream channel;
    channel << res.raw();
    channel >> *con;

    // The negotiated subprotocol should now be reported by the client.
    BOOST_CHECK_EQUAL( con->get_subprotocol(), "foo" );
}

// Per RFC 6455 section 4.1, if the server selects a subprotocol that the
// client did not offer, the client must fail the connection.
BOOST_AUTO_TEST_CASE( unrequested_subprotocol ) {
    client c;
    websocketpp::lib::error_code ec;
    std::stringstream out;

    c.register_ostream(&out);

    connection_ptr con = c.get_connection("ws://localhost/", ec);
    BOOST_CHECK_EQUAL(ec, websocketpp::lib::error_code());
    BOOST_CHECK( con );

    con->add_subprotocol("foo",ec);
    BOOST_CHECK_EQUAL(ec, websocketpp::lib::error_code());

    c.connect(con);

    std::string o = out.str();
    websocketpp::http::parser::request r;
    r.consume(o.data(),o.size(),ec);
    BOOST_CHECK_EQUAL(ec, websocketpp::lib::error_code());
    BOOST_CHECK( r.ready() );

    // Build a valid handshake response, but have the server select a
    // subprotocol ("baz") that the client never offered.
    stub_config::con_msg_manager_type::ptr mm(
        websocketpp::lib::make_shared<stub_config::con_msg_manager_type>());
    stub_config::rng_type rng;
    websocketpp::processor::hybi13<stub_config> proc(false,true,mm,rng);

    websocketpp::http::parser::response res;
    res.set_version("HTTP/1.1");
    res.set_status(websocketpp::http::status_code::switching_protocols);
    ec = proc.process_handshake(r,"baz",res);
    BOOST_CHECK_EQUAL(ec, websocketpp::lib::error_code());

    std::stringstream channel;
    channel << res.raw();
    channel >> *con;

    // The connection must have been failed and no subprotocol stored.
    BOOST_CHECK_EQUAL( con->get_subprotocol(), "" );
    BOOST_CHECK_EQUAL( con->get_ec(),
        websocketpp::error::make_error_code(
            websocketpp::error::unrequested_subprotocol) );
}


