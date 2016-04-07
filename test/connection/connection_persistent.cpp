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
#include <boost/test/unit_test.hpp>

#include <websocketpp/config/core.hpp>
#include <websocketpp/server.hpp>

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

class persistent_config : public websocketpp::config::core
{
public:
  static bool const enable_persistent_connections = true;
};

typedef websocketpp::server<persistent_config> persistent_server;


static std::string run_persistent_server_test(persistent_server & s, std::string input, bool log = false) {
    persistent_server::connection_ptr con;
    std::stringstream output;

    if (log) {
        s.set_access_channels(websocketpp::log::alevel::all);
        s.set_error_channels(websocketpp::log::elevel::all);
    } else {
        s.clear_access_channels(websocketpp::log::alevel::all);
        s.clear_error_channels(websocketpp::log::elevel::all);
    }

    s.register_ostream(&output);

    con = s.get_connection();
    con->start();

    std::stringstream channel;

    channel << input;
    channel >> *con;

    return output.str();
}

static void http_func(persistent_server* s, websocketpp::connection_hdl hdl) {
    using namespace websocketpp::http;

    persistent_server::connection_ptr con = s->get_con_from_hdl(hdl);

    std::string res = con->get_resource();

    con->set_body(res);
    con->set_status(status_code::ok);

    BOOST_CHECK_EQUAL(con->get_response_code(), status_code::ok);
    BOOST_CHECK_EQUAL(con->get_response_msg(), status_code::get_string(status_code::ok));
}

BOOST_AUTO_TEST_CASE( persistent_http_request_should_close_if_requested ) {
    std::string input = "GET /foo/bar HTTP/1.1\r\nHost: www.example.com\r\nConnection: close\r\nOrigin: http://www.example.com\r\n\r\n";
    std::string output = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 8\r\nServer: ";
    output+=websocketpp::user_agent;
    output+="\r\n\r\n/foo/bar";

    persistent_server s;
    s.set_http_handler(bind(&http_func,&s,::_1));

    BOOST_CHECK_EQUAL(run_persistent_server_test(s,input), output);
}

BOOST_AUTO_TEST_CASE( should_not_be_persistent_for_http_10_request ) {
    std::string input = "GET /foo/bar HTTP/1.0\r\nHost: www.example.com\r\nOrigin: http://www.example.com\r\n\r\n";
    std::string output = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 8\r\nServer: ";
    output+=websocketpp::user_agent;
    output+="\r\n\r\n/foo/bar";

    persistent_server s;
    s.set_http_handler(bind(&http_func,&s,::_1));

    BOOST_CHECK_EQUAL(run_persistent_server_test(s,input), output);
}

static void http_func_save_con(persistent_server* s, persistent_server::connection_ptr* c, websocketpp::connection_hdl hdl) {
    using namespace websocketpp::http;

    persistent_server::connection_ptr con = s->get_con_from_hdl(hdl);

    std::string res = con->get_resource();

    con->set_body(res);
    con->set_status(status_code::ok);

    BOOST_CHECK_EQUAL(con->get_response_code(), status_code::ok);
    BOOST_CHECK_EQUAL(con->get_response_msg(), status_code::get_string(status_code::ok));

    *c = con;
}

BOOST_AUTO_TEST_CASE( should_keep_connection_open_and_reset_state ) {
    std::string input = "GET /foo/bar HTTP/1.1\r\nHost: www.example.com\r\nOrigin: http://www.example.com\r\n\r\n";
    std::string output = "HTTP/1.1 200 OK\r\nContent-Length: 8\r\nServer: ";
    output+=websocketpp::user_agent;
    output+="\r\n\r\n/foo/bar";

    persistent_server s;
    persistent_server::connection_ptr c;
    s.set_http_handler(bind(&http_func_save_con,&s,&c,::_1));

    run_persistent_server_test(s, input);

    BOOST_CHECK_EQUAL(websocketpp::session::state::connecting, c->get_state() );
    BOOST_CHECK_EQUAL( "", c->get_host() );
    BOOST_CHECK_EQUAL( "", c->get_resource() );
    BOOST_CHECK_EQUAL( false, c->get_request().ready() );
    BOOST_CHECK_EQUAL( "  \r\n\r\n", c->get_request().raw() );
    BOOST_CHECK_EQUAL( false, c->get_response().ready() );
    BOOST_CHECK_EQUAL( " 0 \r\n\r\n", c->get_response().raw() );
}

