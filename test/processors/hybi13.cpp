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
#define BOOST_TEST_MODULE hybi_13_processor
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>

#include <websocketpp/processors/hybi13.hpp>

#include <websocketpp/http/request.hpp>
#include <websocketpp/http/response.hpp>
#include <websocketpp/message_buffer/message.hpp>
#include <websocketpp/message_buffer/alloc.hpp>

#include <websocketpp/extensions/permessage_compress/disabled.hpp>
#include <websocketpp/extensions/permessage_compress/enabled.hpp>

struct stub_config {
	typedef websocketpp::http::parser::request request_type;
	typedef websocketpp::http::parser::response response_type;

	typedef websocketpp::message_buffer::message
		<websocketpp::message_buffer::alloc::con_msg_manager> message_type;
	typedef websocketpp::message_buffer::alloc::con_msg_manager<message_type> 
		con_msg_manager_type;
    
    struct permessage_compress_config {
        typedef request_type request_type;
    };

    typedef websocketpp::extensions::permessage_compress::disabled
        <permessage_compress_config> permessage_compress_type;

    static const bool enable_extensions = false;
};

struct stub_config_ext {
	typedef websocketpp::http::parser::request request_type;
	typedef websocketpp::http::parser::response response_type;

	typedef websocketpp::message_buffer::message
		<websocketpp::message_buffer::alloc::con_msg_manager> message_type;
	typedef websocketpp::message_buffer::alloc::con_msg_manager<message_type> 
		con_msg_manager_type;
    
    struct permessage_compress_config {
        typedef request_type request_type;
    };
    
    typedef websocketpp::extensions::permessage_compress::enabled
        <permessage_compress_config> permessage_compress_type;

    static const bool enable_extensions = false;
};

BOOST_AUTO_TEST_CASE( exact_match ) {
	stub_config::request_type r;
    stub_config::response_type response;
	stub_config::con_msg_manager_type::ptr msg_manager;
    websocketpp::processor::hybi13<stub_config> p(false,true,msg_manager);
    websocketpp::lib::error_code ec;
    
    std::string handshake = "GET / HTTP/1.1\r\nHost: www.example.com\r\nConnection: upgrade\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n";
    
    r.consume(handshake.c_str(),handshake.size());
    
    BOOST_CHECK(websocketpp::processor::is_websocket_handshake(r));
    BOOST_CHECK(websocketpp::processor::get_websocket_version(r) == p.get_version());
    ec = p.validate_handshake(r);
    BOOST_CHECK(!ec);
    
    websocketpp::uri_ptr u;
    bool exception = false;
    
    try {
    	u = p.get_uri(r);
    } catch (const websocketpp::uri_exception& e) {
    	exception = true;
    }
    
    BOOST_CHECK(exception == false);
    BOOST_CHECK(u->get_secure() == false);
    BOOST_CHECK(u->get_host() == "www.example.com");
    BOOST_CHECK(u->get_resource() == "/");
    BOOST_CHECK(u->get_port() == websocketpp::uri_default_port);
    
    p.process_handshake(r,response);
    
    BOOST_CHECK(response.get_header("Connection") == "upgrade");
    BOOST_CHECK(response.get_header("Upgrade") == "websocket");
    BOOST_CHECK(response.get_header("Sec-WebSocket-Accept") == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
}

BOOST_AUTO_TEST_CASE( non_get_method ) {
	stub_config::request_type r;
    stub_config::response_type response;
	stub_config::con_msg_manager_type::ptr msg_manager;
    websocketpp::processor::hybi13<stub_config> p(false,true,msg_manager);
    websocketpp::lib::error_code ec;

    std::string handshake = "POST / HTTP/1.1\r\nHost: www.example.com\r\nConnection: upgrade\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: foo\r\n\r\n";
    
    r.consume(handshake.c_str(),handshake.size());
    
    BOOST_CHECK(websocketpp::processor::is_websocket_handshake(r));
    BOOST_CHECK(websocketpp::processor::get_websocket_version(r) == p.get_version());
    ec = p.validate_handshake(r);
    BOOST_CHECK( ec == websocketpp::processor::error::invalid_http_method );
}

BOOST_AUTO_TEST_CASE( old_http_version ) {
	stub_config::request_type r;
    stub_config::response_type response;
	stub_config::con_msg_manager_type::ptr msg_manager;
    websocketpp::processor::hybi13<stub_config> p(false,true,msg_manager);
    websocketpp::lib::error_code ec;
    
    std::string handshake = "GET / HTTP/1.0\r\nHost: www.example.com\r\nConnection: upgrade\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: foo\r\n\r\n";
    
    r.consume(handshake.c_str(),handshake.size());
    
    BOOST_CHECK(websocketpp::processor::is_websocket_handshake(r));
    BOOST_CHECK(websocketpp::processor::get_websocket_version(r) == p.get_version());
    ec = p.validate_handshake(r);
    BOOST_CHECK( ec == websocketpp::processor::error::invalid_http_version );
}

BOOST_AUTO_TEST_CASE( missing_handshake_key1 ) {
	stub_config::request_type r;
    stub_config::response_type response;
	stub_config::con_msg_manager_type::ptr msg_manager;
    websocketpp::processor::hybi13<stub_config> p(false,true,msg_manager);
    websocketpp::lib::error_code ec;
    
    std::string handshake = "GET / HTTP/1.1\r\nHost: www.example.com\r\nConnection: upgrade\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 13\r\n\r\n";
    
    r.consume(handshake.c_str(),handshake.size());
    
    BOOST_CHECK(websocketpp::processor::is_websocket_handshake(r));
    BOOST_CHECK(websocketpp::processor::get_websocket_version(r) == p.get_version());
    ec = p.validate_handshake(r);
    BOOST_CHECK( ec == websocketpp::processor::error::missing_required_header );
}

BOOST_AUTO_TEST_CASE( missing_handshake_key2 ) {
	stub_config::request_type r;
    stub_config::response_type response;
	stub_config::con_msg_manager_type::ptr msg_manager;
    websocketpp::processor::hybi13<stub_config> p(false,true,msg_manager);
    websocketpp::lib::error_code ec;
    
    std::string handshake = "GET / HTTP/1.1\r\nHost: www.example.com\r\nConnection: upgrade\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 13\r\n\r\n";
    
    r.consume(handshake.c_str(),handshake.size());
    
    BOOST_CHECK(websocketpp::processor::is_websocket_handshake(r));
    BOOST_CHECK(websocketpp::processor::get_websocket_version(r) == p.get_version());
    ec = p.validate_handshake(r);
    BOOST_CHECK( ec == websocketpp::processor::error::missing_required_header );
}

BOOST_AUTO_TEST_CASE( bad_host ) {
	stub_config::request_type r;
    stub_config::response_type response;
	stub_config::con_msg_manager_type::ptr msg_manager;
    websocketpp::processor::hybi13<stub_config> p(false,true,msg_manager);
    websocketpp::uri_ptr u;
    bool exception = false;
    websocketpp::lib::error_code ec;
    
    std::string handshake = "GET / HTTP/1.1\r\nHost: www.example.com:70000\r\nConnection: upgrade\r\nUpgrade: websocket\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: foo\r\n\r\n";
    
    r.consume(handshake.c_str(),handshake.size());
    
    BOOST_CHECK(websocketpp::processor::is_websocket_handshake(r));
    BOOST_CHECK(websocketpp::processor::get_websocket_version(r) == p.get_version());
    ec = p.validate_handshake(r);
    BOOST_CHECK( !ec );
    
    try {
    	u = p.get_uri(r);
    } catch (const websocketpp::uri_exception& e) {
    	exception = true;
    }
    
    BOOST_CHECK(exception == true);
}

// FRAME TESTS TO DO
//
// unmasked, 0 length, binary
// 0x82 0x00
//
// masked, 0 length, binary
// 0x82 0x80
//
// unmasked, 0 length, text
// 0x81 0x00
//
// masked, 0 length, text
// 0x81 0x80

BOOST_AUTO_TEST_CASE( frame_empty_binary_unmasked ) {
	typedef stub_config::con_msg_manager_type con_msg_manager_type;
	con_msg_manager_type::ptr msg_manager(new con_msg_manager_type());
	uint8_t frame[2] = {0x82, 0x00};
    websocketpp::lib::error_code ec;

	// all in one chunk
	websocketpp::processor::hybi13<stub_config> p1(false,false,msg_manager);

	size_t ret1 = p1.consume(frame,2,ec);
	
	BOOST_CHECK( ret1 == 2 );
	BOOST_CHECK( !ec );
	BOOST_CHECK( p1.ready() == true );	
	
	// two separate chunks
	websocketpp::processor::hybi13<stub_config> p2(false,false,msg_manager);

	BOOST_CHECK( p2.consume(frame,1,ec) == 1 );
	BOOST_CHECK( !ec );
	BOOST_CHECK( p2.ready() == false );

	BOOST_CHECK( p2.consume(frame+1,1,ec) == 1 );
	BOOST_CHECK( !ec );
	BOOST_CHECK( p2.ready() == true );
}

BOOST_AUTO_TEST_CASE( frame_small_binary_unmasked ) {
	typedef stub_config::con_msg_manager_type con_msg_manager_type;
	typedef stub_config::message_type::ptr message_ptr;

	con_msg_manager_type::ptr msg_manager(new con_msg_manager_type());
	websocketpp::processor::hybi13<stub_config> p(false,false,msg_manager);

	uint8_t frame[4] = {0x82, 0x02, 0x2A, 0x2A};
    websocketpp::lib::error_code ec;
	
	BOOST_CHECK( p.get_message() == message_ptr() );
	BOOST_CHECK( p.consume(frame,4,ec) == 4 );
	BOOST_CHECK( !ec );
	BOOST_CHECK( p.ready() == true );
	
	message_ptr foo = p.get_message();

	BOOST_CHECK( p.get_message() == message_ptr() );
	BOOST_CHECK( foo->get_payload() == "**" );

}

BOOST_AUTO_TEST_CASE( frame_extended_binary_unmasked ) {
	typedef stub_config::con_msg_manager_type con_msg_manager_type;
	typedef stub_config::message_type::ptr message_ptr;

	con_msg_manager_type::ptr msg_manager(new con_msg_manager_type());
	websocketpp::processor::hybi13<stub_config> p(false,false,msg_manager);

	uint8_t frame[130] = {0x82, 0x7E, 0x00, 0x7E};
    websocketpp::lib::error_code ec;
	frame[0] = 0x82;
	frame[1] = 0x7E;
	frame[2] = 0x00;
	frame[3] = 0x7E;
	std::fill_n(frame+4,126,0x2A);

	BOOST_CHECK( p.get_message() == message_ptr() );
	BOOST_CHECK( p.consume(frame,130,ec) == 130 );
	BOOST_CHECK( !ec );
	BOOST_CHECK( p.ready() == true );
	
	message_ptr foo = p.get_message();

	BOOST_CHECK( p.get_message() == message_ptr() );
	BOOST_CHECK( foo->get_payload().size() == 126 );
}

BOOST_AUTO_TEST_CASE( frame_jumbo_binary_unmasked ) {
	typedef stub_config::con_msg_manager_type con_msg_manager_type;
	typedef stub_config::message_type::ptr message_ptr;

	con_msg_manager_type::ptr msg_manager(new con_msg_manager_type());
	websocketpp::processor::hybi13<stub_config> p(false,false,msg_manager);

	uint8_t frame[130] = {0x82, 0x7E, 0x00, 0x7E};
    websocketpp::lib::error_code ec;
	std::fill_n(frame+4,126,0x2A);

	BOOST_CHECK( p.get_message() == message_ptr() );
	BOOST_CHECK( p.consume(frame,130,ec) == 130 );
	BOOST_CHECK( !ec );
	BOOST_CHECK( p.ready() == true );
	
	message_ptr foo = p.get_message();

	BOOST_CHECK( p.get_message() == message_ptr() );
	BOOST_CHECK( foo->get_payload().size() == 126 );
}

BOOST_AUTO_TEST_CASE( control_frame_too_large ) {
	using namespace websocketpp::processor;

	typedef stub_config::con_msg_manager_type con_msg_manager_type;
	typedef stub_config::message_type::ptr message_ptr;

	con_msg_manager_type::ptr msg_manager(new con_msg_manager_type());
	websocketpp::processor::hybi13<stub_config> p(false,false,msg_manager);

	uint8_t frame[130] = {0x88, 0x7E, 0x00, 0x7E};
    websocketpp::lib::error_code ec;
	std::fill_n(frame+4,126,0x2A);

	BOOST_CHECK( p.get_message() == message_ptr() );
	BOOST_CHECK( p.consume(frame,130,ec) > 0 );
	BOOST_CHECK( ec == websocketpp::processor::error::control_too_big  );
	BOOST_CHECK( p.ready() == false );
}

BOOST_AUTO_TEST_CASE( rsv_bits_used ) {
	using namespace websocketpp::processor;

	typedef stub_config::con_msg_manager_type con_msg_manager_type;
	typedef stub_config::message_type::ptr message_ptr;

	con_msg_manager_type::ptr msg_manager(new con_msg_manager_type());
	
	uint8_t frame[3][2] = {{0x90, 0x00},
		                  {0xA0, 0x00},
	                      {0xC0, 0x00}};
    websocketpp::lib::error_code ec;

	for (int i = 0; i < 3; i++) {
		websocketpp::processor::hybi13<stub_config> p(false,false,msg_manager);
	
		BOOST_CHECK( p.get_message() == message_ptr() );
		BOOST_CHECK( p.consume(frame[i],2,ec) > 0 );
	    BOOST_CHECK( ec == websocketpp::processor::error::invalid_rsv_bit  );
		BOOST_CHECK( p.ready() == false );
	}
}


BOOST_AUTO_TEST_CASE( reserved_opcode_used ) {
	using namespace websocketpp::processor;

	typedef stub_config::con_msg_manager_type con_msg_manager_type;
	typedef stub_config::message_type::ptr message_ptr;

	con_msg_manager_type::ptr msg_manager(new con_msg_manager_type());
	
	uint8_t frame[10][2] = {{0x83, 0x00},
		                   {0x84, 0x00},
	                       {0x85, 0x00},
	                       {0x86, 0x00},
	                       {0x87, 0x00},
	                       {0x8B, 0x00},
	                       {0x8C, 0x00},
	                       {0x8D, 0x00},
	                       {0x8E, 0x00},
	                       {0x8F, 0x00}};
    websocketpp::lib::error_code ec;

	for (int i = 0; i < 10; i++) {
		websocketpp::processor::hybi13<stub_config> p(false,false,msg_manager);
	
		BOOST_CHECK( p.get_message() == message_ptr() );
		BOOST_CHECK( p.consume(frame[i],2,ec) > 0 );
	    BOOST_CHECK( ec == websocketpp::processor::error::invalid_opcode  );
		BOOST_CHECK( p.ready() == false );
	}
}

BOOST_AUTO_TEST_CASE( fragmented_control_message ) {
	using namespace websocketpp::processor;

	typedef stub_config::con_msg_manager_type con_msg_manager_type;
	typedef stub_config::message_type::ptr message_ptr;

	con_msg_manager_type::ptr msg_manager(new con_msg_manager_type());
	websocketpp::processor::hybi13<stub_config> p(false,false,msg_manager);

	uint8_t frame[2] = {0x08, 0x00};
    websocketpp::lib::error_code ec;

	BOOST_CHECK( p.get_message() == message_ptr() );
	BOOST_CHECK( p.consume(frame,2,ec) > 0 );
	BOOST_CHECK( ec == websocketpp::processor::error::fragmented_control );
	BOOST_CHECK( p.ready() == false );
}

BOOST_AUTO_TEST_CASE( fragmented_binary_message ) {
	using namespace websocketpp::processor;

	typedef stub_config::con_msg_manager_type con_msg_manager_type;
	typedef stub_config::message_type::ptr message_ptr;

	con_msg_manager_type::ptr msg_manager(new con_msg_manager_type());
	websocketpp::processor::hybi13<stub_config> p0(false,false,msg_manager);
	websocketpp::processor::hybi13<stub_config> p1(false,false,msg_manager);

	uint8_t frame0[6] = {0x02, 0x01, 0x2A, 0x80, 0x01, 0x2A};
	uint8_t frame1[8] = {0x02, 0x01, 0x2A, 0x89, 0x00, 0x80, 0x01, 0x2A};
    websocketpp::lib::error_code ec;

	// read fragmented message in one chunk
	BOOST_CHECK( p0.get_message() == message_ptr() );
	BOOST_CHECK( p0.consume(frame0,6,ec) == 6 );
    BOOST_CHECK( !ec );
	BOOST_CHECK( p0.ready() == true );
	BOOST_CHECK( p0.get_message()->get_payload() == "**" );
	
	// read fragmented message in two chunks
	BOOST_CHECK( p0.get_message() == message_ptr() );
	BOOST_CHECK( p0.consume(frame0,3,ec) == 3 );
    BOOST_CHECK( !ec );
	BOOST_CHECK( p0.ready() == false );
	BOOST_CHECK( p0.consume(frame0+3,3,ec) == 3 );
    BOOST_CHECK( !ec );
	BOOST_CHECK( p0.ready() == true );
	BOOST_CHECK( p0.get_message()->get_payload() == "**" );

	// read fragmented message with control message in between
	BOOST_CHECK( p0.get_message() == message_ptr() );
	BOOST_CHECK( p0.consume(frame1,8,ec) == 5 );
    BOOST_CHECK( !ec );
	BOOST_CHECK( p0.ready() == true );
	BOOST_CHECK( p0.get_message()->get_opcode() == websocketpp::frame::opcode::PING);
	BOOST_CHECK( p0.consume(frame1+5,3,ec) == 3 );
    BOOST_CHECK( !ec );
	BOOST_CHECK( p0.ready() == true );
	BOOST_CHECK( p0.get_message()->get_payload() == "**" );

    // read lone continuation frame
	BOOST_CHECK( p0.get_message() == message_ptr() );
	BOOST_CHECK( p0.consume(frame0+3,3,ec) > 0);
    BOOST_CHECK( ec == websocketpp::processor::error::invalid_continuation );

    // read two start frames in a row
	BOOST_CHECK( p1.get_message() == message_ptr() );
	BOOST_CHECK( p1.consume(frame0,3,ec) == 3);
    BOOST_CHECK( !ec );
	BOOST_CHECK( p1.consume(frame0,3,ec) > 0);
    BOOST_CHECK( ec == websocketpp::processor::error::invalid_continuation );
}

BOOST_AUTO_TEST_CASE( unmasked_client_frame ) {
	using namespace websocketpp::processor;

	typedef stub_config::con_msg_manager_type con_msg_manager_type;
	typedef stub_config::message_type::ptr message_ptr;

	con_msg_manager_type::ptr msg_manager(new con_msg_manager_type());
	websocketpp::processor::hybi13<stub_config> p(false,true,msg_manager);

	uint8_t frame[2] = {0x82, 0x00};
    websocketpp::lib::error_code ec;

	BOOST_CHECK( p.get_message() == message_ptr() );
	BOOST_CHECK( p.consume(frame,2,ec) > 0 );
	BOOST_CHECK( ec == websocketpp::processor::error::masking_required );
	BOOST_CHECK( p.ready() == false );
}

BOOST_AUTO_TEST_CASE( masked_server_frame ) {
	using namespace websocketpp::processor;

	typedef stub_config::con_msg_manager_type con_msg_manager_type;
	typedef stub_config::message_type::ptr message_ptr;

	con_msg_manager_type::ptr msg_manager(new con_msg_manager_type());
	websocketpp::processor::hybi13<stub_config> p(false,false,msg_manager);

	uint8_t frame[8] = {0x82, 0x82, 0xFF, 0xFF, 0xFF, 0xFF, 0xD5, 0xD5};
    websocketpp::lib::error_code ec;

	BOOST_CHECK( p.get_message() == message_ptr() );
	BOOST_CHECK( p.consume(frame,8,ec) > 0 );
	BOOST_CHECK( ec == websocketpp::processor::error::masking_forbidden );
	BOOST_CHECK( p.ready() == false );
}

BOOST_AUTO_TEST_CASE( frame_small_binary_masked ) {
	using namespace websocketpp::processor;

	typedef stub_config::con_msg_manager_type con_msg_manager_type;
	typedef stub_config::message_type::ptr message_ptr;

	con_msg_manager_type::ptr msg_manager(new con_msg_manager_type());
	websocketpp::processor::hybi13<stub_config> p(false,true,msg_manager);

	uint8_t frame[8] = {0x82, 0x82, 0xFF, 0xFF, 0xFF, 0xFF, 0xD5, 0xD5};
    websocketpp::lib::error_code ec;

	BOOST_CHECK( p.get_message() == message_ptr() );
	BOOST_CHECK( p.consume(frame,8,ec) == 8 );
    BOOST_CHECK( !ec );
	BOOST_CHECK( p.ready() == true );
	BOOST_CHECK( p.get_message()->get_payload() == "**" );
}

BOOST_AUTO_TEST_CASE( masked_fragmented_binary_message ) {
	using namespace websocketpp::processor;

	typedef stub_config::con_msg_manager_type con_msg_manager_type;
	typedef stub_config::message_type::ptr message_ptr;

	con_msg_manager_type::ptr msg_manager(new con_msg_manager_type());
	websocketpp::processor::hybi13<stub_config> p0(false,true,msg_manager);

	uint8_t frame0[14] = {0x02, 0x81, 0xAB, 0x23, 0x98, 0x45, 0x81, 
	                     0x80, 0x81, 0xB8, 0x34, 0x12, 0xFF, 0x92};
    websocketpp::lib::error_code ec;

	// read fragmented message in one chunk
	BOOST_CHECK( p0.get_message() == message_ptr() );
	BOOST_CHECK( p0.consume(frame0,14,ec) == 14 );
    BOOST_CHECK( !ec );
	BOOST_CHECK( p0.ready() == true );	
	BOOST_CHECK( p0.get_message()->get_payload() == "**" );
}

BOOST_AUTO_TEST_CASE( prepare_data_frame ) {
    // setup
    using namespace websocketpp::processor;

	typedef stub_config::con_msg_manager_type con_msg_manager_type;
	typedef stub_config::message_type::ptr message_ptr;

	con_msg_manager_type::ptr msg_manager(new con_msg_manager_type());
	websocketpp::processor::hybi13<stub_config> p(false,true,msg_manager);
    
    // test
    websocketpp::lib::error_code e;
    message_ptr in = msg_manager->get_message();
    message_ptr out = msg_manager->get_message();
    message_ptr invalid;

    // empty pointers arguements should return sane error 
    e = p.prepare_data_frame(invalid,invalid);
    BOOST_CHECK( e == websocketpp::processor::error::invalid_arguments );

    e = p.prepare_data_frame(in,invalid);
    BOOST_CHECK( e == websocketpp::processor::error::invalid_arguments );

    e = p.prepare_data_frame(invalid,out);
    BOOST_CHECK( e == websocketpp::processor::error::invalid_arguments );

    // test valid opcodes
    // control opcodes should return an error, data ones shouldn't
    for (int i = 0; i < 0xF; i++) {
        in->set_opcode(websocketpp::frame::opcode::value(i));
        
        e = p.prepare_data_frame(in,out);
        
        if (websocketpp::frame::opcode::is_control(in->get_opcode())) {
            BOOST_CHECK( e == websocketpp::processor::error::invalid_opcode );
        } else {
            BOOST_CHECK( e != websocketpp::processor::error::invalid_opcode );
        }
    }
    
    
    //in.set_payload("foo");
    
    //e = prepare_data_frame(in,out);


}
