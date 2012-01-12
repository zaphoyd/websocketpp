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

#ifndef WEBSOCKETPP_URI_HPP
#define WEBSOCKETPP_URI_HPP

#include "common.hpp"

#include <exception>

namespace websocketpp {

// WebSocket URI only (not http/etc) 

class uri_exception : public std::exception {
public: 
    uri_exception(const std::string& msg) : m_msg(msg) {}
    ~uri_exception() throw() {}

    virtual const char* what() const throw() {
        return m_msg.c_str();
    }
private:
    std::string m_msg;
};

// TODO: figure out why this fixes horrible linking errors.
static const uint16_t URI_DEFAULT_PORT = 80;
static const uint16_t URI_DEFAULT_SECURE_PORT = 443;

class uri {
public:
    explicit uri(const std::string& uri);
    uri(bool secure, const std::string& host, uint16_t port, const std::string& resource);
    uri(bool secure, const std::string& host, const std::string& resource);
    uri(bool secure, const std::string& host, const std::string& port, const std::string& resource);
    
    bool get_secure() const;
    std::string get_host() const;
    std::string get_host_port() const;
    uint16_t get_port() const;
    std::string get_port_str() const;
    std::string get_resource() const;
    std::string str() const;
    
    // get query?
    // get fragment
    
    // hi <3
    
    // get the string representation of this URI
    
    //std::string base() const; // is this still needed?
    
    // setter methods set some or all (in the case of parse) based on the input.
    // These functions throw a uri_exception on failure.
    /*void set_uri(const std::string& uri);
    
    void set_secure(bool secure);
    void set_host(const std::string& host);
    void set_port(uint16_t port);
    void set_port(const std::string& port);
    void set_resource(const std::string& resource);*/
private:
    uint16_t get_port_from_string(const std::string& port) const;
    
    bool        m_secure;
    std::string m_host;
    uint16_t    m_port;
    std::string m_resource;
};

typedef boost::shared_ptr<uri> uri_ptr;

} // namespace websocketpp

#endif // WEBSOCKETPP_URI_HPP
