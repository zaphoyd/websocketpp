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
#define BOOST_TEST_MODULE sha1
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>

#include <websocketpp/sha1/sha1.hpp>
#include <websocketpp/utilities.hpp>

BOOST_AUTO_TEST_SUITE ( sha1 )

BOOST_AUTO_TEST_CASE( sha1_test_a ) {
    websocketpp::sha1 sha;
    uint32_t digest[5];

    sha << "abc";
    BOOST_CHECK(sha.get_raw_digest(digest));

    BOOST_CHECK_EQUAL( digest[0], 0xa9993e36 );
    BOOST_CHECK_EQUAL( digest[1], 0x4706816a );
    BOOST_CHECK_EQUAL( digest[2], 0xba3e2571 );
    BOOST_CHECK_EQUAL( digest[3], 0x7850c26c );
    BOOST_CHECK_EQUAL( digest[4], 0x9cd0d89d );
}

BOOST_AUTO_TEST_CASE( sha1_test_b ) {
    websocketpp::sha1 sha;
    uint32_t digest[5];

    sha << "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    BOOST_CHECK(sha.get_raw_digest(digest));

    BOOST_CHECK_EQUAL( digest[0], 0x84983e44 );
    BOOST_CHECK_EQUAL( digest[1], 0x1c3bd26e );
    BOOST_CHECK_EQUAL( digest[2], 0xbaae4aa1 );
    BOOST_CHECK_EQUAL( digest[3], 0xf95129e5 );
    BOOST_CHECK_EQUAL( digest[4], 0xe54670f1 );
}

BOOST_AUTO_TEST_CASE( sha1_test_c ) {
    websocketpp::sha1 sha;
    uint32_t digest[5];

    for (int i = 1; i <= 1000000; i++) {
        sha.input('a');
    }

    BOOST_CHECK(sha.get_raw_digest(digest));

    BOOST_CHECK_EQUAL( digest[0], 0x34aa973c );
    BOOST_CHECK_EQUAL( digest[1], 0xd4c4daa4 );
    BOOST_CHECK_EQUAL( digest[2], 0xf61eeb2b );
    BOOST_CHECK_EQUAL( digest[3], 0xdbad2731 );
    BOOST_CHECK_EQUAL( digest[4], 0x6534016f );
}

BOOST_AUTO_TEST_SUITE_END()
