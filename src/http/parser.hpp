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

#ifndef HTTP_PARSER_HPP
#define HTTP_PARSER_HPP

#include <map>
#include <string>

#include "constants.hpp"

namespace websocketpp {
namespace http {
namespace parser {

	namespace state {
		enum value {
			METHOD,
			RESOURCE,
			VERSION,
			HEADERS
		};
	}

typedef std::map<std::string,std::string> header_list;


class parser {
public:
	// consumes bytes from the stream and returns true if enough bytes have
	// been read
	bool consume (std::istream& s) {
		throw "No Implimented";
	}
	
	void set_version(const std::string& version) {
		// TODO: validation?
		m_version = version;
	}
	
	const std::string& version() const {
		return m_version;
	}
	
	std::string header(const std::string& key) const {
		header_list::const_iterator h = m_headers.find(key);
		
		if (h == m_headers.end()) {
			return "";
		} else {
			return h->second;
		}
	}
	
	// multiple calls to set header will result in values aggregating.
	// use replace_header if you do not want this behavior.
	void set_header(const std::string &key,const std::string &val) {
		// TODO: prevent use of reserved headers?
		if (this->header(key) == "") {
			m_headers[key] = val;
		} else {
			m_headers[key] += ", " + val;
		}
	}
	
	void replace_header(const std::string &key,const std::string &val) {
		m_headers[key] = val;
	}
protected:
	bool parse_headers(std::istream& s) {
		std::string header;
		std::string::size_type end;
		
		// get headers
		while (std::getline(s, header) && header != "\r") {
			if (header[header.size()-1] != '\r') {
				continue; // ignore malformed header lines?
			} else {
				header.erase(header.end()-1);
			}
			
			end = header.find(": ",0);
			
			if (end != std::string::npos) {			
				set_header(header.substr(0,end),header.substr(end+2));
			}
		}
		
		return true;
	}
	
	std::string raw_headers() {
		std::stringstream raw;
		
		header_list::iterator it;
		for (it = m_headers.begin(); it != m_headers.end(); it++) {
			raw << it->first << ": " << it->second << "\r\n";
		}
		
		return raw.str();
	}
	
private:
	std::string m_version;
	header_list m_headers;
};

class request : public parser {
public:	
	// parse a complete header (ie \r\n\r\n MUST be in the input stream)
	bool parse_complete(std::istream& s) {
		std::string request;
		
		// get status line
		std::getline(s, request);
		
		if (request[request.size()-1] == '\r') {
			request.erase(request.end()-1);
			
			std::stringstream ss(request);
			std::string val;
			
			ss >> val;
			set_method(val);
			
			ss >> val;
			set_uri(val);
			
			ss >> val;
			set_version(val);
		} else {
			return false;
		}
		
		return parse_headers(s);
	}
	
	// TODO: validation. Make sure all required fields have been set?
	std::string raw() {
		std::stringstream raw;
		
		raw << m_method << " " << m_uri << " " << version() << "\r\n";
		raw << raw_headers() << "\r\n";
		
		return raw.str();
	}
	
	void set_method(const std::string& method) {
		// TODO: validation?
		m_method = method;
	}
	
	const std::string& method() const {
		return m_method;
	}
	
	void set_uri(const std::string& uri) {
		// TODO: validation?
		m_uri = uri;
	}
	
	const std::string& uri() const {
		return m_uri;
	}
	
private:
	std::string m_method;
	std::string m_uri;
};

class response : public parser {
public:
	// parse a complete header (ie \r\n\r\n MUST be in the input stream)
	bool parse_complete(std::istream& s) {
		std::string response;
		
		// get status line
		std::getline(s, response);
		
		if (response[response.size()-1] == '\r') {
			response.erase(response.end()-1);
			
			std::stringstream	ss(response);
			std::string			str_val;
			int					int_val;
			
			ss >> str_val;
			set_version(str_val);
			
			ss >> int_val;
			set_status(status_code::value(int_val),str_val);
		} else {
			return false;
		}
		
		return parse_headers(s);
	}
	
	// TODO: validation. Make sure all required fields have been set?
	std::string raw() {
		std::stringstream raw;
		
		raw << version() << " " << m_status_code << " " << m_status_msg << "\r\n";
		raw << raw_headers() << "\r\n";
		
		return raw.str();
	}
	
	void set_status(status_code::value code) {
		// TODO: validation?
		m_status_code = code;
		m_status_msg = get_string(code);
	}
	
	void set_status(status_code::value code, const std::string& msg) {
		// TODO: validation?
		m_status_code = code;
		m_status_msg = msg;
	}
	
	status_code::value status_code() const {
		return m_status_code;
	}
	
	const std::string& status_msg() const {
		return m_status_msg;
	}
private:
	status_code::value	m_status_code;
	std::string			m_status_msg;
};

}
}
}

#endif // HTTP_PARSER_HPP
