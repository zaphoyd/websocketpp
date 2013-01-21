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
#define BOOST_TEST_MODULE basic_logger
#include <boost/test/unit_test.hpp>

#include <string>

#include <websocketpp/logger/basic.hpp>
#include <websocketpp/concurrency/none.hpp>

BOOST_AUTO_TEST_CASE( is_token_char ) {
    typedef websocketpp::logger::basic<websocketpp::concurrency::none,websocketpp::logger::error_names> error_logger;
    
    error_logger elog;
    
    BOOST_CHECK( elog.static_test(websocketpp::logger::error_names::info ) == true );
    BOOST_CHECK( elog.static_test(websocketpp::logger::error_names::warn ) == true );
    BOOST_CHECK( elog.static_test(websocketpp::logger::error_names::rerror ) == true );
    BOOST_CHECK( elog.static_test(websocketpp::logger::error_names::fatal ) == true );
    
    elog.set_channels(websocketpp::logger::error_names::info);
    
    elog.write(websocketpp::logger::error_names::info,"Information");
    elog.write(websocketpp::logger::error_names::warn,"A warning");
    elog.write(websocketpp::logger::error_names::rerror,"A error");
    elog.write(websocketpp::logger::error_names::fatal,"A critical error");
}
