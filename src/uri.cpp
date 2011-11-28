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

#include <boost/regex.hpp>

using websocketpp::uri;

uri::uri(const std::string& uri) {
	boost::cmatch matches;
	static const boost::regex expression("(ws|wss)://([^/:\\[]+|\\[[0-9:]+\\])(:\\d{1,5})?(/[^#]*)?");
	
	// TODO: should this split resource into path/query?
	
	if (boost::regex_match(uri.c_str(), matches, expression)) {
		m_secure = (matches[1] == "wss");
		m_host = matches[2];
		
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

uint16_t uri::get_port() const {
	return m_port;
}

std::string uri::get_resource() const {
	return m_resource;
}

std::string uri::str() const {
	std::stringstream s;
	
	s << "ws" << (m_secure ? "s" : "") << "://" << m_host;
	
	/*if (m_port != (m_secure ? DEFAULT_SECURE_PORT : DEFAULT_PORT)) {
		s << ":" << m_port;
	}*/
	
	s << m_resource;
	return s.str();
}

uint16_t uri::get_port_from_string(const std::string& port) const {
	if (port == "") {
	    return (m_secure ? DEFAULT_SECURE_PORT : DEFAULT_PORT);
	}
	
	unsigned int t_port = atoi(port.c_str());
			
	if (t_port > 65535) {
		throw websocketpp::uri_exception("Port must be less than 65535");
	}
	
	if (t_port == 0) {
		throw websocketpp::uri_exception("Error parsing port string");
	}
	
	return t_port;
}

/*std::string uri::base() const {
	std::stringstream s;
	
	s << "ws" << (secure ? "s" : "") << "://" << host;
	
	if (port != (secure ? DEFAULT_SECURE_PORT : DEFAULT_PORT)) {
		s << ":" << port;
	}
	
	s << "/";
	return s.str();
}

void uri::set_secure(bool secure) {
	m_secure = secure;
}

void uri::set_host(const std::string& host) {
	// TODO: additional validation?
	m_host = host;
}
void set_port(uint16_t port) {
	if (port > 65535) {
		throw uri_exception("Port must be less than 65535");
	}
}
void set_port(const std::string& port);
void set_resource(const std::string& resource);*/













/*bool uri::parse(const std::string& uri) {
	boost::cmatch matches;
	static const boost::regex expression("(ws|wss)://([^/:\\[]+|\\[[0-9:]+\\])(:\\d{1,5})?(/[^#]*)?");
	
	// TODO: should this split resource into path/query?
	
	if (boost::regex_match(uri.c_str(), matches, expression)) {
		secure = (matches[1] == "wss");
		host = matches[2];
		
		std::string port(matches[3]);
		
		if (port != "") {
		    // strip off the :
		    // this could probably be done with a better regex.
		    port = port.substr(1);
		}
		
		m_port = get_port_from_string(port);
		
		if (matches[3] == "") {
			port = (secure ? DEFAULT_SECURE_PORT : DEFAULT_PORT);
		} else {
			unsigned int t_port = atoi(std::string(matches[3]).substr(1).c_str());
			
			if (t_port > 65535) {
				return false;
			}
			
			port = atoi(std::string(matches[3]).substr(1).c_str());
		}
		
		resource = (matches[4] == "" ? "/" : matches[4]);
		
		return true;
	}
	
	return false;
}*/



