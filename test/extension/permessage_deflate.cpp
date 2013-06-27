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
#define BOOST_TEST_MODULE permessage_deflate
#include <boost/test/unit_test.hpp>

#include <websocketpp/error.hpp>

#include <websocketpp/extensions/extension.hpp>
#include <websocketpp/extensions/permessage_deflate/disabled.hpp>
#include <websocketpp/extensions/permessage_deflate/enabled.hpp>

#include <string>

class config {};

typedef websocketpp::extensions::permessage_deflate::enabled<config> enabled_type;
typedef websocketpp::extensions::permessage_deflate::disabled<config> disabled_type;

struct ext_vars {
    enabled_type exts;
    enabled_type extc;
    websocketpp::err_str_pair esp;
    websocketpp::http::attribute_list attr;
};
namespace pmde = websocketpp::extensions::permessage_deflate::error;

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

BOOST_AUTO_TEST_CASE( negotiate_s2c_no_context_takeover_invalid ) {
    ext_vars v;
    v.attr["s2c_no_context_takeover"] = "foo";
    
    v.esp = v.exts.negotiate(v.attr); 
    BOOST_CHECK( !v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.esp.first, pmde::make_error_code(pmde::invalid_attribute_value) );
    BOOST_CHECK_EQUAL( v.esp.second, "");
}

BOOST_AUTO_TEST_CASE( negotiate_s2c_no_context_takeover ) {
    ext_vars v;
    v.attr["s2c_no_context_takeover"] = "";
    
    v.esp = v.exts.negotiate(v.attr); 
    BOOST_CHECK( v.exts.is_enabled() );
    BOOST_CHECK_EQUAL( v.esp.first, websocketpp::lib::error_code() );
    BOOST_CHECK_EQUAL( v.esp.second, "permessage-deflate; s2c_no_context_takeover");
}
