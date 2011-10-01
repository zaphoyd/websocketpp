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
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE parsing
#include <boost/test/unit_test.hpp>

#include "../../src/websocketpp.hpp"

// Test a regular valid ws URI
BOOST_AUTO_TEST_CASE( uri_valid ) {
	websocketpp::ws_uri uri;

	BOOST_CHECK( uri.parse("ws://localhost:9000/chat") == true);
	BOOST_CHECK( uri.secure == false );
	BOOST_CHECK( uri.host == "localhost");
	BOOST_CHECK( uri.port == 9000 );
	BOOST_CHECK( uri.resource == "/chat" );
}

// Valid URI with no port specified (unsecure)
BOOST_AUTO_TEST_CASE( uri_valid_no_port_unsecure ) {
	websocketpp::ws_uri uri;

	BOOST_CHECK( uri.parse("ws://localhost/chat") == true);
	BOOST_CHECK( uri.secure == false );
	BOOST_CHECK( uri.host == "localhost");
	BOOST_CHECK( uri.port == 80 );
	BOOST_CHECK( uri.resource == "/chat" );
}

// Valid URI with no port (secure)
BOOST_AUTO_TEST_CASE( uri_valid_no_port_secure ) {
	websocketpp::ws_uri uri;

	BOOST_CHECK( uri.parse("wss://localhost/chat") == true);
	BOOST_CHECK( uri.secure == true );
	BOOST_CHECK( uri.host == "localhost");
	BOOST_CHECK( uri.port == 443 );
	BOOST_CHECK( uri.resource == "/chat" );
}

// Valid URI with no resource
BOOST_AUTO_TEST_CASE( uri_valid_no_resource ) {
	websocketpp::ws_uri uri;

	BOOST_CHECK( uri.parse("ws://localhost:9000") == true);
	BOOST_CHECK( uri.secure == false );
	BOOST_CHECK( uri.host == "localhost");
	BOOST_CHECK( uri.port == 9000 );
	BOOST_CHECK( uri.resource == "/" );
}

// Valid URI IPv6 Literal
BOOST_AUTO_TEST_CASE( uri_valid_ipv6_literal ) {
	websocketpp::ws_uri uri;

	BOOST_CHECK( uri.parse("wss://[::1]:9000/chat") == true);
	BOOST_CHECK( uri.secure == true );
	BOOST_CHECK( uri.host == "[::1]");
	BOOST_CHECK( uri.port == 9000 );
	BOOST_CHECK( uri.resource == "/chat" );
}

// Valid URI with more complicated host
BOOST_AUTO_TEST_CASE( uri_valid_2 ) {
	websocketpp::ws_uri uri;

	BOOST_CHECK( uri.parse("wss://thor-websocket.zaphoyd.net:88/") == true);
	BOOST_CHECK( uri.secure == true );
	BOOST_CHECK( uri.host == "thor-websocket.zaphoyd.net");
	BOOST_CHECK( uri.port == 88 );
	BOOST_CHECK( uri.resource == "/" );
}

// Invalid URI (port too long)
BOOST_AUTO_TEST_CASE( uri_invalid_long_port ) {
	websocketpp::ws_uri uri;

	BOOST_CHECK( uri.parse("wss://localhost:900000/chat") == false);
}

// INvalid URI (http method)
BOOST_AUTO_TEST_CASE( uri_invalid_http ) {
	websocketpp::ws_uri uri;

	BOOST_CHECK( uri.parse("http://localhost:9000/chat") == false);
}

// Valid URI IPv4 literal
BOOST_AUTO_TEST_CASE( uri_valid_ipv4_literal ) {
	websocketpp::ws_uri uri;

	BOOST_CHECK( uri.parse("wss://127.0.0.1:9000/chat") == true);
	BOOST_CHECK( uri.secure == true );
	BOOST_CHECK( uri.host == "127.0.0.1");
	BOOST_CHECK( uri.port == 9000 );
	BOOST_CHECK( uri.resource == "/chat" );
}

// Valid URI complicated resource path
BOOST_AUTO_TEST_CASE( uri_valid_3 ) {
	websocketpp::ws_uri uri;

	BOOST_CHECK( uri.parse("wss://localhost:9000/chat/foo/bar") == true);
	BOOST_CHECK( uri.secure == true );
	BOOST_CHECK( uri.host == "localhost");
	BOOST_CHECK( uri.port == 9000 );
	BOOST_CHECK( uri.resource == "/chat/foo/bar" );
}

// Invalid URI broken method separator
BOOST_AUTO_TEST_CASE( uri_invalid_method_separator ) {
	websocketpp::ws_uri uri;

	BOOST_CHECK( uri.parse("wss:/localhost:9000/chat") == false);
}

// Invalid URI port > 65535
BOOST_AUTO_TEST_CASE( uri_invalid_gt_16_bit_port ) {
	websocketpp::ws_uri uri;

	BOOST_CHECK( uri.parse("wss:/localhost:70000/chat") == false);
}

// Invalid URI includes uri fragment
BOOST_AUTO_TEST_CASE( uri_invalid_fragment ) {
	websocketpp::ws_uri uri;

	BOOST_CHECK( uri.parse("wss:/localhost:70000/chat#foo") == false);
}

// Valid URI complicated resource path with query
BOOST_AUTO_TEST_CASE( uri_valid_4 ) {
	websocketpp::ws_uri uri;

	BOOST_CHECK( uri.parse("wss://localhost:9000/chat/foo/bar?foo=bar") == true);
	BOOST_CHECK( uri.secure == true );
	BOOST_CHECK( uri.host == "localhost");
	BOOST_CHECK( uri.port == 9000 );
	BOOST_CHECK( uri.resource == "/chat/foo/bar?foo=bar" );
}

