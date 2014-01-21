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

#ifndef WEBSOCKETPP_PROCESSOR_HTTP11_HPP
#define WEBSOCKETPP_PROCESSOR_HTTP11_HPP

#include <cstdlib>

#include <websocketpp/frame.hpp>
#include <websocketpp/common/network.hpp>

#include <websocketpp/processors/processor.hpp>

namespace websocketpp {
namespace processor {

template <typename config>
class http11 : public processor<config> {
public:
    typedef processor<config> base;

    typedef typename config::request_type request_type;
    typedef typename config::response_type response_type;

    typedef typename config::message_type message_type;
    typedef typename message_type::ptr message_ptr;

    typedef typename config::con_msg_manager_type::ptr msg_manager_ptr;

    explicit http11(bool secure, bool server, msg_manager_ptr manager)
      : processor<config>(secure, server)
      , m_msg_manager(manager) {}

    int get_version() const {
        return -1;
    }

    bool is_websocket() const {
      return false;
    }

    lib::error_code validate_handshake(request_type const & r) const {
        if (r.get_method() != "GET") {
            return make_error_code(error::invalid_http_method);
        }

        if (r.get_version() != "HTTP/1.1") {
            return make_error_code(error::invalid_http_version);
        }

        return lib::error_code();
    }

    lib::error_code process_handshake(request_type const & req,
        std::string const & subprotocol, response_type & res) const
    {
        return error::make_error_code(error::no_protocol_support);
    }

    lib::error_code client_handshake_request(request_type& req, uri_ptr
        uri, std::vector<std::string> const & subprotocols) const
    {
        req.set_method("GET");
        req.set_uri(uri->get_resource());
        req.set_version("HTTP/1.1");

        req.append_header("Connection","Close");
        req.replace_header("Host",uri->get_host_port());
        return lib::error_code();
    }

    lib::error_code validate_server_handshake_response(request_type const & req,
        response_type & res) const
    {
        return lib::error_code();
    }

    std::string get_raw(response_type const & res) const {
        return res.raw();
    }

    std::string const & get_origin(request_type const & r) const {
        return r.get_header("Origin");
    }

    // http doesn't support subprotocols so there never will be any requested
    lib::error_code extract_subprotocols(request_type const & req,
        std::vector<std::string> & subprotocol_list)
    {
        return lib::error_code();
    }

    uri_ptr get_uri(request_type const & request) const {
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

    /// Process new websocket connection bytes
    size_t consume(uint8_t * buf, size_t len, lib::error_code & ec) {
        ec = lib::error_code(error::no_protocol_support);
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
        return lib::error_code(error::no_protocol_support);
    }

    lib::error_code prepare_ping(std::string const & in, message_ptr out) const
    {
        return lib::error_code(error::no_protocol_support);
    }

    lib::error_code prepare_pong(const std::string & in, message_ptr out) const
    {
        return lib::error_code(error::no_protocol_support);
    }

    lib::error_code prepare_close(close::status::value code,
        std::string const & reason, message_ptr out) const
    {
        return lib::error_code(error::no_protocol_support);
    }

private:
    msg_manager_ptr m_msg_manager;
};

} // namespace processor
} // namespace websocketpp

#endif //WEBSOCKETPP_PROCESSOR_HTTP11_HPP
