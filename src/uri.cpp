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

#include "uri.hpp"

#include <sstream>
#include <iostream>

#include <boost/regex.hpp>

using websocketpp::uri;

uri::uri(const std::string& uri) {
    // temporary testing non-regex implimentation.
    /*enum state {
        BEGIN = 0,
        HOST_BEGIN = 1,
        READ_IPV6_LITERAL = 2,
        READ_HOSTNAME = 3,
        BEGIN_PORT = 4,
        READ_PORT = 5,
        READ_RESOURCE = 6,
    };
    
    state the_state = BEGIN;
    std::string temp;
    
    for (std::string::const_iterator it = uri.begin(); it != uri.end(); it++) {
        switch (the_state) {
            case BEGIN:
                // we are looking for a ws:// or wss://
                if (temp.size() < 6) {
                    temp.append(1,*it);
                } else {
                    throw websocketpp::uri_exception("Scheme is too long");
                }
                                
                if (temp == "ws://") {
                    m_secure = false;
                    the_state = HOST_BEGIN;
                    temp.clear();
                } else if (temp == "wss://") {
                    m_secure = true;
                    the_state = HOST_BEGIN;
                    temp.clear();
                }
                break;
            case HOST_BEGIN:
                // if character is [ go into IPv6 literal state
                // otherwise go into read hostname state
                if (*it == '[') {
                    the_state = READ_IPV6_LITERAL;
                } else {
                    *it--;
                    the_state = READ_HOSTNAME;
                }
                break;
            case READ_IPV6_LITERAL:
                // read 0-9a-fA-F:. until ] or 45 characters have been read
                if (*it == ']') {
                    the_state = BEGIN_PORT;
                    break;
                }
                
                if (m_host.size() > 45) {
                    throw websocketpp::uri_exception("IPv6 literal is too long");
                }
                
                if (*it == '.' ||
                    *it == ':' ||
                    (*it >= '0' && *it <= '9') ||
                    (*it >= 'a' && *it <= 'f') ||
                    (*it >= 'A' && *it <= 'F'))
                {
                    m_host.append(1,*it);
                }
                break;
            case READ_HOSTNAME:
                //
                if (*it == ':') {
                    it--;
                    the_state = BEGIN_PORT;
                    break;
                }
                
                if (*it == '/') {
                    it--;
                    the_state = BEGIN_PORT;
                    break;
                }
                
                // TODO: max url length?
                
                // TODO: check valid characters?
                if (1) {
                    m_host.append(1,*it);
                }
                break;
            case BEGIN_PORT:
                if (*it == ':') {
                    the_state = READ_PORT;
                } else if (*it == '/') {
                    m_port = get_port_from_string("");
                     *it--;
                    the_state = READ_RESOURCE;
                } else {
                    throw websocketpp::uri_exception("Error parsing WebSocket URI");
                }
                break;
            case READ_PORT:
                if (*it >= '0' && *it <= '9') {
                    if (temp.size() < 5) {
                        temp.append(1,*it);
                    } else {
                        throw websocketpp::uri_exception("Port is too long");
                    }
                } else if (*it == '/') {
                    m_port = get_port_from_string(temp);
                    temp.clear();
                     *it--;
                    the_state = READ_RESOURCE;
                    
                } else {
                    throw websocketpp::uri_exception("Error parsing WebSocket URI");
                }
                break;
            case READ_RESOURCE:
                // max length check?
                
                if (*it == '#') {
                    throw websocketpp::uri_exception("WebSocket URIs cannot have fragments");
                } else {
                    m_resource.append(1,*it);
                }
                break;
        }
    }
    
    switch (the_state) {
        case READ_PORT:
            m_port = get_port_from_string(temp);
            break;
        default:
            break;
    }
    
    if (m_resource == "") {
        m_resource = "/";
    }*/
    
    
    boost::cmatch matches;
    const boost::regex expression("(ws|wss)://([^/:\\[]+|\\[[0-9a-fA-F:.]+\\])(:\\d{1,5})?(/[^#]*)?");
    
    // TODO: should this split resource into path/query?
    
    if (boost::regex_match(uri.c_str(), matches, expression)) {
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
    
}

uri::uri(bool secure, const std::string& host, uint16_t port, const std::string& resource) 
 : m_secure(secure),
   m_host(host), 
   m_port(port),
   m_resource(resource == "" ? "/" : resource) {}

uri::uri(bool secure, const std::string& host, const std::string& resource) 
: m_secure(secure),
  m_host(host), 
  m_port(m_secure ? URI_DEFAULT_SECURE_PORT : URI_DEFAULT_PORT),
  m_resource(resource == "" ? "/" : resource) {}

uri::uri(bool secure, 
         const std::string& host, 
         const std::string& port, 
         const std::string& resource) 
 : m_secure(secure),
   m_host(host),
   m_port(get_port_from_string(port)),
   m_resource(resource == "" ? "/" : resource) {}

/* Slightly cleaner C++11 delegated constructor method

uri::uri(bool secure, 
         const std::string& host, 
         uint16_t port, 
         const std::string& resource) 
 : m_secure(secure),
   m_host(host), 
   m_port(port),
   m_resource(resource == "" ? "/" : resource)
{
    if (m_port > 65535) {
        throw websocketpp::uri_exception("Port must be less than 65535");
    }
}

uri::uri(bool secure, 
         const std::string& host, 
         const std::string& port, 
         const std::string& resource) 
 : uri(secure, host, get_port_from_string(port), resource) {}

*/

bool uri::get_secure() const {
    return m_secure;
}

std::string uri::get_host() const {
    return m_host;
}

std::string uri::get_host_port() const {
    if (m_port == (m_secure ? URI_DEFAULT_SECURE_PORT : URI_DEFAULT_PORT)) {
         return m_host;
    } else {
        std::stringstream p;
        p << m_host << ":" << m_port;
        return p.str();
    }
    
}

uint16_t uri::get_port() const {
    return m_port;
}

std::string uri::get_port_str() const {
    std::stringstream p;
    p << m_port;
    return p.str();
}

std::string uri::get_resource() const {
    return m_resource;
}

std::string uri::str() const {
    std::stringstream s;
    
    s << "ws" << (m_secure ? "s" : "") << "://" << m_host;
    
    if (m_port != (m_secure ? URI_DEFAULT_SECURE_PORT : URI_DEFAULT_PORT)) {
        s << ":" << m_port;
    }
    
    s << m_resource;
    return s.str();
}

uint16_t uri::get_port_from_string(const std::string& port) const {
    if (port == "") {
        return (m_secure ? URI_DEFAULT_SECURE_PORT : URI_DEFAULT_PORT);
    }
    
    unsigned int t_port = atoi(port.c_str());
            
    if (t_port > 65535) {
        throw websocketpp::uri_exception("Port must be less than 65535");
    }
    
    if (t_port == 0) {
        throw websocketpp::uri_exception("Error parsing port string: "+port);
    }
    
    return static_cast<uint16_t>(t_port);
}
