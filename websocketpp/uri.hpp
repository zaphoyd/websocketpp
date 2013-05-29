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

#ifndef WEBSOCKETPP_URI_HPP
#define WEBSOCKETPP_URI_HPP

#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/regex.hpp>

#include <exception>
#include <iostream>
#include <sstream>
#include <string>

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
static const uint16_t uri_default_port = 80;
static const uint16_t URI_DEFAULT_SECURE_PORT = 443;
static const uint16_t uri_default_secure_port = 443;

class uri {
public:
    explicit uri(const std::string& uri) {
        // TODO: should this split resource into path/query?
        lib::cmatch matches;
        const lib::regex expression("(http|https|ws|wss)://([^/:\\[]+|\\[[0-9a-fA-F:.]+\\])(:\\d{1,5})?(/[^#]*)?");
        
        if (lib::regex_match(uri.c_str(), matches, expression)) {
            m_scheme = matches[1];
            m_secure = (m_scheme == "wss" || m_scheme == "https");
            m_host = matches[2];
            
            // strip brackets from IPv6 literal URIs
            if (m_host[0] == '[') {
                m_host = m_host.substr(1,m_host.size()-2);
            }
            
            std::string port(matches[3]);
            
            if (port != "") {
                // strip off the :
                // this could probably be done with a better regex.
                port = port.substr(1);
            }
            
            m_port = get_port_from_string(port);
            
            m_resource = matches[4];
            
            if (m_resource == "") {
                m_resource = "/";
            }
            
            return;
        }
        
        throw websocketpp::uri_exception("Error parsing WebSocket URI");
    }
    
    /*explicit uri(const std::string& uri) {
        // test for ws or wss
        std::string::const_iterator it;
        std::string::const_iterator temp;
        
        it = uri.begin();
        
        if (std::equal(it,it+6,"wss://")) {
            m_secure = true;
            it += 6;
        } else if (std::equal(it,it+5,"ws://")) {
            m_secure = false;
            it += 5;
        } else {
            // error
        }
        
        // extract host.
        // either a host string
        // an IPv4 address
        // or an IPv6 address
        if (*it == '[') {
            ++it;
            // IPv6 literal
            // extract IPv6 digits until ]
            temp = std::find(it,uri.end(),']');
            if (temp == uri.end()) {
                // error
            } else {
                // validate IPv6 literal parts
                // can contain numbers, a-f and A-F
            }
        } else {
            // IPv4 or hostname
        }
        
        // TODO: should this split resource into path/query?
        lib::cmatch matches;
        const lib::regex expression("(ws|wss)://([^/:\\[]+|\\[[0-9a-fA-F:.]+\\])(:\\d{1,5})?(/[^#]*)?");
        
        if (lib::regex_match(uri.c_str(), matches, expression)) {
            m_secure = (matches[1] == "wss");
            m_host = matches[2];
            
            // strip brackets from IPv6 literal URIs
            if (m_host[0] == '[') {
                m_host = m_host.substr(1,m_host.size()-2);
            }
            
            std::string port(matches[3]);
            
            if (port != "") {
                // strip off the :
                // this could probably be done with a better regex.
                port = port.substr(1);
            }
            
            m_port = get_port_from_string(port);
            
            m_resource = matches[4];
            
            if (m_resource == "") {
                m_resource = "/";
            }
            
            return;
        }
        
        throw websocketpp::uri_exception("Error parsing WebSocket URI");

    }*/
    
    uri(bool secure, const std::string& host, uint16_t port, const std::string& resource)
      : m_scheme(secure ? "wss" : "ws")
      , m_host(host)
      , m_resource(resource == "" ? "/" : resource)
      , m_port(port)
      , m_secure(secure) {}
    
    uri(bool secure, const std::string& host, const std::string& resource)
      : m_scheme(secure ? "wss" : "ws")
      , m_host(host)
      , m_resource(resource == "" ? "/" : resource)
      , m_port(secure ? uri_default_secure_port : uri_default_port)
      , m_secure(secure) {}
      
    uri(bool secure, const std::string& host, const std::string& port, const std::string& resource)
      : m_scheme(secure ? "wss" : "ws")
      , m_host(host)
      , m_resource(resource == "" ? "/" : resource)
      , m_port(get_port_from_string(port))
      , m_secure(secure) {}
    
    
    uri(std::string scheme, const std::string& host, uint16_t port, const std::string& resource)
      : m_scheme(scheme)
      , m_host(host)
      , m_resource(resource == "" ? "/" : resource)
      , m_port(port)
      , m_secure(scheme == "wss" || scheme == "https") {}
    
    uri(std::string scheme, const std::string& host, const std::string& resource)
      : m_scheme(scheme)
      , m_host(host)
      , m_resource(resource == "" ? "/" : resource)
      , m_port((scheme == "wss" || scheme == "https") ? uri_default_secure_port : uri_default_port)
      , m_secure(scheme == "wss" || scheme == "https") {}
      
    uri(std::string scheme, const std::string& host, const std::string& port, const std::string& resource)
      : m_scheme(scheme)
      , m_host(host)
      , m_resource(resource == "" ? "/" : resource)
      , m_port(get_port_from_string(port))
      , m_secure(scheme == "wss" || scheme == "https") {}
    
    bool get_secure() const {
        return m_secure;
    }
    
    const std::string& get_host() const {
        return m_host;
    }
    
    std::string get_host_port() const {
        if (m_port == (m_secure ? uri_default_secure_port : uri_default_port)) {
            return m_host;
        } else {
            std::stringstream p;
            p << m_host << ":" << m_port;
            return p.str();
        }
    }
    
    std::string get_authority() const {
        std::stringstream p;
        p << m_host << ":" << m_port;
        return p.str();
    }
    
    uint16_t get_port() const {
        return m_port;
    }
    
    std::string get_port_str() const {
        std::stringstream p;
        p << m_port;
        return p.str();
    }
    
    const std::string& get_resource() const {
        return m_resource;
    }
    
    std::string str() const {
        std::stringstream s;
    
        s << m_scheme << "://" << m_host;
        
        if (m_port != (m_secure ? uri_default_secure_port : uri_default_port)) {
            s << ":" << m_port;
        }
        
        s << m_resource;
        return s.str();
    }
    
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
    uint16_t get_port_from_string(const std::string& port) const {
        if (port == "") {
            return (m_secure ? uri_default_secure_port : uri_default_port);
        }
        
        unsigned int t_port = static_cast<unsigned int>(atoi(port.c_str()));
                
        if (t_port > 65535) {
            throw websocketpp::uri_exception("Port must be less than 65535");
        }
        
        if (t_port == 0) {
            throw websocketpp::uri_exception("Error parsing port string: "+port);
        }
        
        return static_cast<uint16_t>(t_port);
    }
    
    std::string m_scheme;
    std::string m_host;
    std::string m_resource;
    uint16_t    m_port;
    bool        m_secure;
};

typedef lib::shared_ptr<uri> uri_ptr;

} // namespace websocketpp

#endif // WEBSOCKETPP_URI_HPP
