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
#define BOOST_TEST_MODULE permessage_deflate
#include <boost/test/unit_test.hpp>

#include <websocketpp/error.hpp>

#include <websocketpp/extensions/extension.hpp>
#include <websocketpp/extensions/permessage_deflate/disabled.hpp>
#include <websocketpp/extensions/permessage_deflate/enabled.hpp>

#include <string>

#include <websocketpp/utilities.hpp>
#include <iostream>

class config {
public:
    static const size_t max_message_size = 16u * 1024u * 1024u;
};

typedef websocketpp::extensions::permessage_deflate::enabled<config> enabled_type;
typedef websocketpp::extensions::permessage_deflate::disabled<config> disabled_type;

struct ext_vars {
    enabled_type exts;
    enabled_type extc;
    websocketpp::lib::error_code ec;
    websocketpp::err_str_pair esp;
    websocketpp::http::attribute_list attr;
};
namespace pmde = websocketpp::extensions::permessage_deflate::error;
namespace pmd_mode = websocketpp::extensions::permessage_deflate::mode;

static std::string make_deterministic_message(size_t size) {
    std::string message(size, '\0');
    unsigned int state = 0x12345678u;

    for (size_t i = 0; i < size; ++i) {
        state = state * 1664525u + 1013904223u;
        message[i] = static_cast<char>(state >> 24);
    }

    return message;
}

// Ensure the disabled extension behaves appropriately disabled

BOOST_AUTO_TEST_CASE( disabled_is_disabled ) {
    disabled_type exts;
    BOOST_CHECK( !exts.is_implemented() );
}

BOOST_AUTO_TEST_CASE( disabled_is_off ) {
    disabled_type exts;
    BOOST_CHECK( !exts.is_enabled() );
}

// Ensure the enabled version actually works

BOOST_AUTO_TEST_CASE( enabled_is_enabled ) {
    ext_vars v;
    BOOST_CHECK( v.exts.is_implemented() );
    BOOST_CHECK( v.extc.is_implemented() );
}


BOOST_AUTO_TEST_CASE( enabled_starts_disabled ) {
    ext_vars v;
    BOOST_CHECK( !v.exts.is_enabled() );
    BOOST_CHECK( !v.extc.is_enabled() );
}

BOOST_AUTO_TEST_CASE( negotiation_empty_attr ) {
    ext_vars v;

    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate");
}

BOOST_AUTO_TEST_CASE( negotiation_invalid_attr ) {
    ext_vars v;
    v.attr["foo"] = "bar";

    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( !v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.esp.first, pmde::make_error_code(pmde::invalid_attributes) );
    BOOST_CHECK_EQUAL( v.esp.second, "");
}

// Negotiate server_no_context_takeover
BOOST_AUTO_TEST_CASE( negotiate_server_no_context_takeover_invalid ) {
    ext_vars v;
    v.attr["server_no_context_takeover"] = "foo";

    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( !v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.esp.first, pmde::make_error_code(pmde::invalid_attribute_value) );
    BOOST_CHECK_EQUAL( v.esp.second, "");
}

BOOST_AUTO_TEST_CASE( negotiate_server_no_context_takeover ) {
    ext_vars v;
    v.attr["server_no_context_takeover"].clear();

    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; server_no_context_takeover");
}

BOOST_AUTO_TEST_CASE( negotiate_server_no_context_takeover_server_initiated ) {
    ext_vars v;

    v.exts.enable_server_no_context_takeover();
    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; server_no_context_takeover");
}

// Negotiate client_no_context_takeover
BOOST_AUTO_TEST_CASE( negotiate_client_no_context_takeover_invalid ) {
    ext_vars v;
    v.attr["client_no_context_takeover"] = "foo";

    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( !v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.esp.first, pmde::make_error_code(pmde::invalid_attribute_value) );
    BOOST_CHECK_EQUAL( v.esp.second, "");
}

BOOST_AUTO_TEST_CASE( negotiate_client_no_context_takeover ) {
    ext_vars v;
    v.attr["client_no_context_takeover"].clear();

    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; client_no_context_takeover");
}

BOOST_AUTO_TEST_CASE( negotiate_client_no_context_takeover_server_initiated ) {
    ext_vars v;

    v.exts.enable_client_no_context_takeover();
    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; client_no_context_takeover");
}


// Negotiate server_max_window_bits
BOOST_AUTO_TEST_CASE( negotiate_server_max_window_bits_invalid ) {
    ext_vars v;

    std::vector<std::string> values;
    values.push_back("");
    values.push_back("foo");
    values.push_back("7");
    values.push_back("16");

    std::vector<std::string>::const_iterator it;
    for (it = values.begin(); it != values.end(); ++it) {
        v.attr["server_max_window_bits"] = *it;

        v.esp = v.exts.negotiate(v.attr);
        BOOST_CHECK( !v.exts.is_enabled() );
        BOOST_CHECK_EQUAL( v.esp.first, pmde::make_error_code(pmde::invalid_attribute_value) );
        BOOST_CHECK_EQUAL( v.esp.second, "");
    }
}

BOOST_AUTO_TEST_CASE( negotiate_server_max_window_bits_valid ) {
    ext_vars v;
    
    // confirm that a request for a value of 8 is interpreted as 9
    v.attr["server_max_window_bits"] = "8";
    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; server_max_window_bits=9");

    v.attr["server_max_window_bits"] = "9";
    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; server_max_window_bits=9");


    v.attr["server_max_window_bits"] = "15";
    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate");
}

BOOST_AUTO_TEST_CASE( invalid_set_server_max_window_bits ) {
    ext_vars v;

    v.ec = v.exts.set_server_max_window_bits(7,pmd_mode::decline);
    BOOST_CHECK_EQUAL(v.ec,pmde::make_error_code(pmde::invalid_max_window_bits));

    v.ec = v.exts.set_server_max_window_bits(16,pmd_mode::decline);
    BOOST_CHECK_EQUAL(v.ec,pmde::make_error_code(pmde::invalid_max_window_bits));
}

BOOST_AUTO_TEST_CASE( negotiate_server_max_window_bits_decline ) {
    ext_vars v;
    v.attr["server_max_window_bits"] = "9";

    v.ec = v.exts.set_server_max_window_bits(15,pmd_mode::decline);
    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate");
}

BOOST_AUTO_TEST_CASE( negotiate_server_max_window_bits_accept_8 ) {
    ext_vars v;
    v.attr["server_max_window_bits"] = "8";

    v.ec = v.exts.set_server_max_window_bits(15,pmd_mode::accept);
    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; server_max_window_bits=9");
}

BOOST_AUTO_TEST_CASE( negotiate_server_max_window_bits_accept ) {
    ext_vars v;
    v.attr["server_max_window_bits"] = "9";

    v.ec = v.exts.set_server_max_window_bits(15,pmd_mode::accept);
    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; server_max_window_bits=9");
}

BOOST_AUTO_TEST_CASE( negotiate_server_max_window_bits_largest_8 ) {
    ext_vars v;
    v.attr["server_max_window_bits"] = "8";

    v.ec = v.exts.set_server_max_window_bits(15,pmd_mode::largest);
    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; server_max_window_bits=9");
}

BOOST_AUTO_TEST_CASE( negotiate_server_max_window_bits_largest ) {
    ext_vars v;
    v.attr["server_max_window_bits"] = "9";

    v.ec = v.exts.set_server_max_window_bits(15,pmd_mode::largest);
    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; server_max_window_bits=9");
}

BOOST_AUTO_TEST_CASE( negotiate_server_max_window_bits_smallest_8 ) {
    ext_vars v;
    v.attr["server_max_window_bits"] = "8";

    v.ec = v.exts.set_server_max_window_bits(15,pmd_mode::smallest);
    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; server_max_window_bits=9");
}

BOOST_AUTO_TEST_CASE( negotiate_server_max_window_bits_smallest ) {
    ext_vars v;
    v.attr["server_max_window_bits"] = "9";

    v.ec = v.exts.set_server_max_window_bits(15,pmd_mode::smallest);
    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; server_max_window_bits=9");
}

// Negotiate server_max_window_bits
BOOST_AUTO_TEST_CASE( negotiate_client_max_window_bits_invalid ) {
    ext_vars v;

    std::vector<std::string> values;
    values.push_back("foo");
    values.push_back("7");
    values.push_back("16");

    std::vector<std::string>::const_iterator it;
    for (it = values.begin(); it != values.end(); ++it) {
        v.attr["client_max_window_bits"] = *it;

        v.esp = v.exts.negotiate(v.attr);
        BOOST_CHECK( !v.exts.is_enabled() );
        BOOST_CHECK_EQUAL( v.esp.first, pmde::make_error_code(pmde::invalid_attribute_value) );
        BOOST_CHECK_EQUAL( v.esp.second, "");
    }
}

BOOST_AUTO_TEST_CASE( negotiate_client_max_window_bits_valid ) {
    ext_vars v;

    v.attr["client_max_window_bits"].clear();
    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate");

    v.attr["client_max_window_bits"] = "8";
    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; client_max_window_bits=9");

    v.attr["client_max_window_bits"] = "9";
    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; client_max_window_bits=9");

    v.attr["client_max_window_bits"] = "15";
    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate");
}

BOOST_AUTO_TEST_CASE( invalid_set_client_max_window_bits ) {
    ext_vars v;

    v.ec = v.exts.set_client_max_window_bits(7,pmd_mode::decline);
    BOOST_CHECK_EQUAL(v.ec,pmde::make_error_code(pmde::invalid_max_window_bits));

    v.ec = v.exts.set_client_max_window_bits(16,pmd_mode::decline);
    BOOST_CHECK_EQUAL(v.ec,pmde::make_error_code(pmde::invalid_max_window_bits));
}

BOOST_AUTO_TEST_CASE( negotiate_client_max_window_bits_decline_8 ) {
    ext_vars v;
    v.attr["client_max_window_bits"] = "8";

    v.ec = v.exts.set_client_max_window_bits(8,pmd_mode::decline);
    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate");
}

BOOST_AUTO_TEST_CASE( negotiate_client_max_window_bits_decline ) {
    ext_vars v;
    v.attr["client_max_window_bits"] = "9";

    v.ec = v.exts.set_client_max_window_bits(9,pmd_mode::decline);
    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate");
}

BOOST_AUTO_TEST_CASE( negotiate_client_max_window_bits_accept_8 ) {
    ext_vars v;
    v.attr["client_max_window_bits"] = "8";

    v.ec = v.exts.set_client_max_window_bits(15,pmd_mode::accept);
    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; client_max_window_bits=9");
}

BOOST_AUTO_TEST_CASE( negotiate_client_max_window_bits_accept ) {
    ext_vars v;
    v.attr["client_max_window_bits"] = "9";

    v.ec = v.exts.set_client_max_window_bits(15,pmd_mode::accept);
    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; client_max_window_bits=9");
}

BOOST_AUTO_TEST_CASE( negotiate_client_max_window_bits_largest_8 ) {
    ext_vars v;
    v.attr["client_max_window_bits"] = "8";

    v.ec = v.exts.set_client_max_window_bits(15,pmd_mode::largest);
    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; client_max_window_bits=9");
}

BOOST_AUTO_TEST_CASE( negotiate_client_max_window_bits_largest ) {
    ext_vars v;
    v.attr["client_max_window_bits"] = "9";

    v.ec = v.exts.set_client_max_window_bits(15,pmd_mode::largest);
    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; client_max_window_bits=9");
}

BOOST_AUTO_TEST_CASE( negotiate_client_max_window_bits_smallest_8 ) {
    ext_vars v;
    v.attr["client_max_window_bits"] = "8";

    v.ec = v.exts.set_client_max_window_bits(15,pmd_mode::smallest);
    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; client_max_window_bits=9");
}

BOOST_AUTO_TEST_CASE( negotiate_client_max_window_bits_smallest ) {
    ext_vars v;
    v.attr["client_max_window_bits"] = "9";

    v.ec = v.exts.set_client_max_window_bits(15,pmd_mode::smallest);
    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; client_max_window_bits=9");
}


// Combinations with 2
BOOST_AUTO_TEST_CASE( negotiate_two_client_initiated1 ) {
    ext_vars v;

    v.attr["server_no_context_takeover"].clear();
    v.attr["client_no_context_takeover"].clear();

    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; server_no_context_takeover; client_no_context_takeover");
}

BOOST_AUTO_TEST_CASE( negotiate_two_client_initiated2 ) {
    ext_vars v;

    v.attr["server_no_context_takeover"].clear();
    v.attr["server_max_window_bits"] = "10";

    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; server_no_context_takeover; server_max_window_bits=10");
}

BOOST_AUTO_TEST_CASE( negotiate_two_client_initiated3 ) {
    ext_vars v;

    v.attr["server_no_context_takeover"].clear();
    v.attr["client_max_window_bits"] = "10";

    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; server_no_context_takeover; client_max_window_bits=10");
}

BOOST_AUTO_TEST_CASE( negotiate_two_client_initiated4 ) {
    ext_vars v;

    v.attr["client_no_context_takeover"].clear();
    v.attr["server_max_window_bits"] = "10";

    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; client_no_context_takeover; server_max_window_bits=10");
}

BOOST_AUTO_TEST_CASE( negotiate_two_client_initiated5 ) {
    ext_vars v;

    v.attr["client_no_context_takeover"].clear();
    v.attr["client_max_window_bits"] = "10";

    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; client_no_context_takeover; client_max_window_bits=10");
}

BOOST_AUTO_TEST_CASE( negotiate_two_client_initiated6 ) {
    ext_vars v;

    v.attr["server_max_window_bits"] = "10";
    v.attr["client_max_window_bits"] = "10";

    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; server_max_window_bits=10; client_max_window_bits=10");
}

BOOST_AUTO_TEST_CASE( negotiate_three_client_initiated1 ) {
    ext_vars v;

    v.attr["server_no_context_takeover"].clear();
    v.attr["client_no_context_takeover"].clear();
    v.attr["server_max_window_bits"] = "10";

    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; server_no_context_takeover; client_no_context_takeover; server_max_window_bits=10");
}

BOOST_AUTO_TEST_CASE( negotiate_three_client_initiated2 ) {
    ext_vars v;

    v.attr["server_no_context_takeover"].clear();
    v.attr["client_no_context_takeover"].clear();
    v.attr["client_max_window_bits"] = "10";

    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; server_no_context_takeover; client_no_context_takeover; client_max_window_bits=10");
}

BOOST_AUTO_TEST_CASE( negotiate_three_client_initiated3 ) {
    ext_vars v;

    v.attr["server_no_context_takeover"].clear();
    v.attr["server_max_window_bits"] = "10";
    v.attr["client_max_window_bits"] = "10";

    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; server_no_context_takeover; server_max_window_bits=10; client_max_window_bits=10");
}

BOOST_AUTO_TEST_CASE( negotiate_three_client_initiated4 ) {
    ext_vars v;

    v.attr["client_no_context_takeover"].clear();
    v.attr["server_max_window_bits"] = "10";
    v.attr["client_max_window_bits"] = "10";

    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; client_no_context_takeover; server_max_window_bits=10; client_max_window_bits=10");
}

BOOST_AUTO_TEST_CASE( negotiate_four_client_initiated ) {
    ext_vars v;

    v.attr["server_no_context_takeover"].clear();
    v.attr["client_no_context_takeover"].clear();
    v.attr["server_max_window_bits"] = "10";
    v.attr["client_max_window_bits"] = "10";

    v.esp = v.exts.negotiate(v.attr);
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; server_no_context_takeover; client_no_context_takeover; server_max_window_bits=10; client_max_window_bits=10");
}

// Compression
BOOST_AUTO_TEST_CASE( compress_data ) {
    ext_vars v;

    std::string compress_in = "Hello";
    std::string compress_out;
    std::string decompress_out;

    v.ec = v.exts.init(true);
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );

    v.ec = v.exts.compress(compress_in,compress_out);
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );

    v.ec = v.exts.decompress(reinterpret_cast<const uint8_t *>(compress_out.data()),compress_out.size(),decompress_out);
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( compress_in, decompress_out );
}

BOOST_AUTO_TEST_CASE( compress_data_multiple ) {
    ext_vars v;

    v.ec = v.exts.init(true);
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );

    for (int i = 0; i < 2; i++) {
        std::string compress_in = "Hello";
        std::string compress_out;
        std::string decompress_out;

        v.ec = v.exts.compress(compress_in,compress_out);
        BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );

        v.ec = v.exts.decompress(reinterpret_cast<const uint8_t *>(compress_out.data()),compress_out.size(),decompress_out);
        BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );
        BOOST_CHECK_EQUAL( compress_in, decompress_out );
    }
}

BOOST_AUTO_TEST_CASE( compress_data_large ) {
    ext_vars v;

    std::string compress_in(600,'*');
    std::string compress_out;
    std::string decompress_out;

    websocketpp::http::attribute_list alist;

    alist["server_max_window_bits"] = "9";
    v.exts.set_server_max_window_bits(9,websocketpp::extensions::permessage_deflate::mode::smallest);

    v.exts.negotiate(alist);
    v.ec = v.exts.init(true);
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );

    v.ec = v.exts.compress(compress_in,compress_out);
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );

    v.ec = v.exts.decompress(reinterpret_cast<const uint8_t *>(compress_out.data()),compress_out.size(),decompress_out);
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( compress_in, decompress_out );
}

BOOST_AUTO_TEST_CASE( compress_data_no_context_takeover ) {
    ext_vars v;

    std::string compress_in = "Hello";
    std::string compress_out1;
    std::string compress_out2;
    std::string decompress_out;

    websocketpp::http::attribute_list alist;

    alist["server_no_context_takeover"].clear();
    v.exts.enable_server_no_context_takeover();

    v.exts.negotiate(alist);
    v.ec = v.exts.init(true);
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );

    v.ec = v.exts.compress(compress_in,compress_out1);
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );

    v.ec = v.exts.decompress(
        reinterpret_cast<const uint8_t *>(compress_out1.data()),
        compress_out1.size(),
        decompress_out
    );
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( compress_in, decompress_out );

    decompress_out.clear();

    v.ec = v.exts.compress(compress_in,compress_out2);
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );

    v.ec = v.exts.decompress(
        reinterpret_cast<const uint8_t *>(compress_out2.data()),
        compress_out2.size(),
        decompress_out
    );
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( compress_in, decompress_out );

    BOOST_CHECK_EQUAL( compress_out1, compress_out2 );
}

BOOST_AUTO_TEST_CASE( compress_data_client_no_context_takeover_offer_without_response_param ) {
    ext_vars v;
    enabled_type second_message_server;

    std::string offer = v.extc.generate_offer();
    std::string compress_in = make_deterministic_message(4096);
    std::string compress_out1;
    std::string compress_out2;
    std::string decompress_out1;
    std::string decompress_out2;
    websocketpp::http::attribute_list response_without_client_no_context_takeover;

    BOOST_CHECK( offer.find("client_no_context_takeover") != std::string::npos );

    v.ec = v.extc.validate_offer(response_without_client_no_context_takeover);
    BOOST_REQUIRE_EQUAL( v.ec, websocketpp::lib::error_code() );

    v.ec = v.extc.init(false);
    BOOST_REQUIRE_EQUAL( v.ec, websocketpp::lib::error_code() );

    v.ec = v.exts.init(true);
    BOOST_REQUIRE_EQUAL( v.ec, websocketpp::lib::error_code() );

    v.ec = second_message_server.init(true);
    BOOST_REQUIRE_EQUAL( v.ec, websocketpp::lib::error_code() );

    v.ec = v.extc.compress(compress_in,compress_out1);
    BOOST_REQUIRE_EQUAL( v.ec, websocketpp::lib::error_code() );

    v.ec = v.extc.compress(compress_in,compress_out2);
    BOOST_REQUIRE_EQUAL( v.ec, websocketpp::lib::error_code() );

    v.ec = v.exts.decompress(
        reinterpret_cast<const uint8_t *>(compress_out1.data()),
        compress_out1.size(),
        decompress_out1
    );
    BOOST_REQUIRE_EQUAL( v.ec, websocketpp::lib::error_code() );
    BOOST_CHECK( decompress_out1 == compress_in );

    v.ec = second_message_server.decompress(
        reinterpret_cast<const uint8_t *>(compress_out2.data()),
        compress_out2.size(),
        decompress_out2
    );
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );
    if (!v.ec) {
        BOOST_CHECK( decompress_out2 == compress_in );
    }
}

BOOST_AUTO_TEST_CASE( compress_empty ) {
    ext_vars v;

    std::string compress_in;
    std::string compress_out;
    std::string decompress_out;

    v.ec = v.exts.init(true);
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );

    v.ec = v.exts.compress(compress_in,compress_out);
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );

    v.ec = v.exts.decompress(reinterpret_cast<const uint8_t *>(compress_out.data()),compress_out.size(),decompress_out);

    compress_out.clear();
    decompress_out.clear();

    v.ec = v.exts.compress(compress_in,compress_out);
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );

    v.ec = v.exts.decompress(reinterpret_cast<const uint8_t *>(compress_out.data()),compress_out.size(),decompress_out);
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( compress_in, decompress_out );
}

/// @todo: more compression tests
/**
 * - compress at different compression levels
 */

// Decompression
BOOST_AUTO_TEST_CASE( decompress_data ) {
    ext_vars v;

    uint8_t in[11] = {0xf2, 0x48, 0xcd, 0xc9, 0xc9, 0x07, 0x00, 0x00, 0x00, 0xff, 0xff};
    std::string out;
    std::string reference = "Hello";

    v.ec = v.exts.init(true);
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );

    v.ec = v.exts.decompress(in,11,out);

    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( out, reference );
}

BOOST_AUTO_TEST_CASE( decompress_data_at_size_limit ) {
    ext_vars v;
    size_t const limit = config::max_message_size;

    std::string compress_in(limit, '*');
    std::string compress_out;
    std::string decompress_out;

    v.ec = v.exts.init(true);
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );

    v.ec = v.exts.compress(compress_in,compress_out);
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );

    v.ec = v.exts.decompress(
        reinterpret_cast<const uint8_t *>(compress_out.data()),
        compress_out.size(),
        decompress_out
    );

    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( decompress_out.size(), limit );
    BOOST_CHECK_EQUAL( decompress_out, compress_in );
}

BOOST_AUTO_TEST_CASE( decompress_data_over_size_limit ) {
    ext_vars v;
    size_t const limit = config::max_message_size;

    std::string compress_in(limit + 1, '*');
    std::string compress_out;
    std::string decompress_out;

    v.ec = v.exts.init(true);
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );

    v.ec = v.exts.compress(compress_in,compress_out);
    BOOST_CHECK_EQUAL( v.ec, websocketpp::lib::error_code() );

    v.ec = v.exts.decompress(
        reinterpret_cast<const uint8_t *>(compress_out.data()),
        compress_out.size(),
        decompress_out
    );

    BOOST_CHECK( v.ec );
    // Contract: never append past the configured limit. The exact size on
    // overflow is implementation-defined; only require the upper bound.
    BOOST_CHECK_LE( decompress_out.size(), limit );
    // Whatever was appended must be a valid prefix of the original payload.
    BOOST_CHECK_EQUAL( decompress_out.find_first_not_of('*'), std::string::npos );
}

// Verify the limit is enforced relative to the current size of `out`, not
// just the bytes appended by this call. The extension is invoked once per
// fragment with `out` accumulating across calls, so the limit budget must
// account for prior contents.
BOOST_AUTO_TEST_CASE( decompress_with_pre_populated_out ) {
    ext_vars v;
    size_t const limit = config::max_message_size;
    size_t const prefill = limit / 2;

    // Compress a payload that, on its own, would fit within the limit but
    // when combined with the pre-existing contents of `out` would exceed it.
    std::string compress_in(prefill + 1, '*');
    std::string compress_out;
    std::string decompress_out(prefill, 'x');

    v.ec = v.exts.init(true);
    BOOST_REQUIRE_EQUAL( v.ec, websocketpp::lib::error_code() );

    v.ec = v.exts.compress(compress_in,compress_out);
    BOOST_REQUIRE_EQUAL( v.ec, websocketpp::lib::error_code() );

    v.ec = v.exts.decompress(
        reinterpret_cast<const uint8_t *>(compress_out.data()),
        compress_out.size(),
        decompress_out
    );

    BOOST_CHECK_EQUAL( v.ec, pmde::make_error_code(pmde::message_too_big) );
    BOOST_CHECK_LE( decompress_out.size(), limit );
}

// Edge case: a zero-byte limit. Any non-empty decompression output should
// fail without crashing or underflowing the remaining-budget calculation.
BOOST_AUTO_TEST_CASE( decompress_with_zero_limit ) {
    ext_vars v;
    std::string compress_in(64, '*');
    std::string compress_out;
    std::string decompress_out;

    v.ec = v.exts.init(true);
    BOOST_REQUIRE_EQUAL( v.ec, websocketpp::lib::error_code() );

    v.ec = v.exts.compress(compress_in,compress_out);
    BOOST_REQUIRE_EQUAL( v.ec, websocketpp::lib::error_code() );

    v.exts.set_max_message_size(0);

    v.ec = v.exts.decompress(
        reinterpret_cast<const uint8_t *>(compress_out.data()),
        compress_out.size(),
        decompress_out
    );

    BOOST_CHECK_EQUAL( v.ec, pmde::make_error_code(pmde::message_too_big) );
    BOOST_CHECK_EQUAL( decompress_out.size(), 0u );
}

// Edge case: a one-byte limit. Verifies the +1 overflow-detection trick
// still rejects oversized payloads when the budget is at its smallest
// non-zero value.
BOOST_AUTO_TEST_CASE( decompress_with_one_byte_limit ) {
    ext_vars v;
    std::string compress_in(64, '*');
    std::string compress_out;
    std::string decompress_out;

    v.ec = v.exts.init(true);
    BOOST_REQUIRE_EQUAL( v.ec, websocketpp::lib::error_code() );

    v.ec = v.exts.compress(compress_in,compress_out);
    BOOST_REQUIRE_EQUAL( v.ec, websocketpp::lib::error_code() );

    v.exts.set_max_message_size(1);

    v.ec = v.exts.decompress(
        reinterpret_cast<const uint8_t *>(compress_out.data()),
        compress_out.size(),
        decompress_out
    );

    BOOST_CHECK_EQUAL( v.ec, pmde::make_error_code(pmde::message_too_big) );
    BOOST_CHECK_LE( decompress_out.size(), 1u );
}
