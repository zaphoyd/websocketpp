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

#include <stdint.h>
#include <string>
#include <boost/regex.hpp>

namespace websocketpp {

struct uri {
	bool parse(const std::string& uri) {
		boost::cmatch what;
		static const boost::regex expression("(ws|wss)://([^/:\\[]+|\\[[0-9:]+\\])(:\\d{1,5})?(/[^#]*)?");
		
		// TODO: should this split resource into path/query?
		
		if (boost::regex_match(uri.c_str(), what, expression)) {
			if (what[1] == "wss") {
				secure = true;
			} else {
				secure = false;
			}
			
			host = what[2];
			
			if (what[3] == "") {
				port = (secure ? DEFAULT_SECURE_PORT : DEFAULT_PORT);
			} else {
				unsigned int t_port = atoi(std::string(what[3]).substr(1).c_str());
				
				if (t_port > 65535) {
					return false;
				}
				
				port = atoi(std::string(what[3]).substr(1).c_str());
			}
			
			if (what[4] == "") {
				resource = "/";
			} else {
				resource = what[4];
			}
			
			return true;
		} else {
			return false;
		}
	}
	std::string base() {
		std::stringstream s;
		
		s << "ws" << (secure ? "s" : "") << "://" << host;
		
		if (port != (secure ? DEFAULT_SECURE_PORT : DEFAULT_PORT)) {
			s << ":" << port;
		}
		
		s << "/";
		return s.str();

	}
	std::string str() {
		std::stringstream s;
		
		s << "ws" << (secure ? "s" : "") << "://" << host;
		
		if (port != (secure ? DEFAULT_SECURE_PORT : DEFAULT_PORT)) {
			s << ":" << port;
		}
		
		s << resource;
		return s.str();
	}
	
	bool		secure;
	std::string	host;
	uint16_t	port;
	std::string	resource;
};

} // namespace websocketpp

#endif // WEBSOCKETPP_URI_HPP
