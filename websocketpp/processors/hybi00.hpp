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

#ifndef WEBSOCKETPP_PROCESSOR_HYBI00_HPP
#define WEBSOCKETPP_PROCESSOR_HYBI00_HPP

#include <cstdlib>

// For htonl
#if defined(WIN32)
	#include <winsock2.h>
#else
	#include <arpa/inet.h>
#endif

#include <websocketpp/processors/processor.hpp>

#include <websocketpp/md5/md5.hpp>

namespace websocketpp {
namespace processor {

/// Processor for Hybi Draft version 00
/**
 * There are many differences between Hybi 00 and Hybi 13
 */
template <typename config>
class hybi00 : public processor<config> {
public:
    typedef processor<config> base;
    
	typedef typename config::request_type request_type;
	typedef typename config::response_type response_type;
	
	typedef typename config::message_type message_type;
	typedef typename message_type::ptr message_ptr;

	typedef typename config::con_msg_manager_type::ptr msg_manager_ptr;

    explicit hybi00(bool secure, bool server, msg_manager_ptr manager) 
      : processor<config>(secure, server) {}
    
    int get_version() const {
        return 0;
    }
    
    lib::error_code validate_handshake(const request_type& r) const {
        if (r.get_method() != "GET") {
            return make_error_code(error::invalid_http_method);
        }
        
        if (r.get_version() != "HTTP/1.1") {
            return make_error_code(error::invalid_http_version);
        }
        
        // required headers
        // Host is required by HTTP/1.1
        // Connection is required by is_websocket_handshake
        // Upgrade is required by is_websocket_handshake
        if (r.get_header("Sec-WebSocket-Key1") == "" ||
            r.get_header("Sec-WebSocket-Key2") == "" ||
            r.get_header("Sec-WebSocket-Key3") == "")
        {
            return make_error_code(error::missing_required_header);
        }

        return lib::error_code();
    }
    
    lib::error_code process_handshake(const request_type& req, const
        std::string & subprotocol, response_type& res) const
    {
        char key_final[16];
        
        // copy key1 into final key
        decode_client_key(req.get_header("Sec-WebSocket-Key1"), &key_final[0]);
                
        // copy key2 into final key
        decode_client_key(req.get_header("Sec-WebSocket-Key2"), &key_final[4]);
        
        // copy key3 into final key
        // key3 should be exactly 8 bytes. If it is more it will be truncated
        // if it is less the final key will almost certainly be wrong.
        // TODO: decide if it is best to silently fail here or produce some sort
        //       of warning or exception.
        const std::string& key3 = req.get_header("Sec-WebSocket-Key3");
        std::copy(key3.c_str(),
                  key3.c_str()+std::min(static_cast<size_t>(8), key3.size()),
                  &key_final[8]);
        
        res.append_header(
            "Sec-WebSocket-Key3",
            md5::md5_hash_string(std::string(key_final,16))
        );
        
        res.append_header("Upgrade","websocket");
        res.append_header("Connection","Upgrade");
        
        // Echo back client's origin unless our local application set a
        // more restrictive one.
        if (res.get_header("Sec-WebSocket-Origin") == "") {
            res.append_header("Sec-WebSocket-Origin",req.get_header("Origin"));
        }
        
        // Echo back the client's request host unless our local application
        // set a different one.
        if (res.get_header("Sec-WebSocket-Location") == "") {
            uri_ptr uri = get_uri(req);
            res.append_header("Sec-WebSocket-Location",uri->str());
        }
        
        return lib::error_code();
    }
    
    // outgoing client connection processing is not supported for this version
    lib::error_code client_handshake_request(request_type& req, uri_ptr uri, 
        const std::vector<std::string> & subprotocols) const
    {
        return error::make_error_code(error::no_protocol_support);
    }
    
    lib::error_code validate_server_handshake_response(const request_type& req,
        response_type& res) const
    {
        return error::make_error_code(error::no_protocol_support);
    }
    
    std::string get_raw(const response_type& res) const {
        return res.raw() + res.get_header("Sec-WebSocket-Key3");
    }
    
    const std::string& get_origin(const request_type& r) const {
        return r.get_header("Origin");
    }
    
    // hybi00 doesn't support subprotocols so there never will be any requested
    lib::error_code extract_subprotocols(const request_type & req,
        std::vector<std::string> & subprotocol_list)
    {
        return lib::error_code();
    }
    
    uri_ptr get_uri(const request_type& request) const {
        std::string h = request.get_header("Host");
        
        size_t last_colon = h.rfind(":");
        size_t last_sbrace = h.rfind("]");
        
        // no : = hostname with no port
        // last : before ] = ipv6 literal with no port
        // : with no ] = hostname with port
        // : after ] = ipv6 literal with port
                
        if (last_colon == std::string::npos || 
            (last_sbrace != std::string::npos && last_sbrace > last_colon))
        {
            return uri_ptr(new uri(base::m_secure, h, request.get_uri()));
        } else {
            return uri_ptr(new uri(base::m_secure,
                                   h.substr(0,last_colon),
                                   h.substr(last_colon+1),
                                   request.get_uri()));
        }
        
        // TODO: check if get_uri is a full uri
    }
    
    std::string get_key3() const {
        return "";
    }

	/// Process new websocket connection bytes
	size_t consume(uint8_t * buf, size_t len, lib::error_code & ec) {
        ec = make_error_code(error::not_implimented);
		return 0;
	}

	bool ready() const {
		return false;
	}
	
	bool get_error() const {
		return false;
	}

	message_ptr get_message() {
		return message_ptr();
	}
	
	/// Prepare a message for writing
	/**
	 * Performs validation, masking, compression, etc. will return an error if 
	 * there was an error, otherwise msg will be ready to be written
	 */
	virtual lib::error_code prepare_data_frame(message_ptr in, message_ptr out)
	{
		// assert msg
		
		// check if the message is prepared already
		
		// validate opcode
		// validate payload utf8
		
		// if we are a client generate a masking key
		
		// generate header
		// perform compression
		// perform masking
		
		return lib::error_code();
	}

    lib::error_code prepare_ping(const std::string & in, message_ptr out) const
    {
        return lib::error_code(error::no_protocol_support);
    }

    lib::error_code prepare_pong(const std::string & in, message_ptr out) const
    {
        return lib::error_code(error::no_protocol_support);
    }
    
    lib::error_code prepare_close(close::status::value code, 
        const std::string & reason, message_ptr out) const
    {
        return lib::error_code(error::no_protocol_support);
    }
private:
    void decode_client_key(const std::string& key, char* result) const {
        int spaces = 0;
        std::string digits = "";
        uint32_t num;
        
        // key2
        for (size_t i = 0; i < key.size(); i++) {
            if (key[i] == ' ') {
                spaces++;
            } else if (key[i] >= '0' && key[i] <= '9') {
                digits += key[i];
            }
        }
        
        num = strtoul(digits.c_str(), NULL, 10);
        if (spaces > 0 && num > 0) {
            num = htonl(num/spaces);
            std::copy(reinterpret_cast<char*>(&num),
                      reinterpret_cast<char*>(&num)+4,
                      result);
        } else {
            std::fill(result,result+4,0);
        }
    }
};


} // namespace processor
} // namespace websocketpp

#endif //WEBSOCKETPP_PROCESSOR_HYBI00_HPP
