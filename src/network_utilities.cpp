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

#include "network_utilities.hpp"

uint64_t htonll(uint64_t src) { 
	static int typ = TYP_INIT; 
	unsigned char c; 
	union { 
		uint64_t ull; 
		unsigned char c[8]; 
	} x; 
	if (typ == TYP_INIT) { 
		x.ull = 0x01; 
		typ = (x.c[7] == 0x01ULL) ? TYP_BIGE : TYP_SMLE; 
	} 
	if (typ == TYP_BIGE) 
		return src; 
	x.ull = src; 
	c = x.c[0]; x.c[0] = x.c[7]; x.c[7] = c; 
	c = x.c[1]; x.c[1] = x.c[6]; x.c[6] = c; 
	c = x.c[2]; x.c[2] = x.c[5]; x.c[5] = c; 
	c = x.c[3]; x.c[3] = x.c[4]; x.c[4] = c; 
	return x.ull; 
}

uint64_t ntohll(uint64_t src) { 
	return htonll(src);
}

std::string lookup_http_error_string(int code) {
	switch (code) {
		case 400:
			return "Bad Request";
		case 401:
			return "Unauthorized";
		case 403:
			return "Forbidden";
		case 404:
			return "Not Found";
		case 405:
			return "Method Not Allowed";
		case 406:
			return "Not Acceptable";
		case 407:
			return "Proxy Authentication Required";
		case 408:
			return "Request Timeout";
		case 409:
			return "Conflict";
		case 410:
			return "Gone";
		case 411:
			return "Length Required";
		case 412:
			return "Precondition Failed";
		case 413:
			return "Request Entity Too Large";
		case 414:
			return "Request-URI Too Long";
		case 415:
			return "Unsupported Media Type";
		case 416:
			return "Requested Range Not Satisfiable";
		case 417:
			return "Expectation Failed";
		case 500:
			return "Internal Server Error";
		case 501:
			return "Not Implimented";
		case 502:
			return "Bad Gateway";
		case 503:
			return "Service Unavailable";
		case 504:
			return "Gateway Timeout";
		case 505:
			return "HTTP Version Not Supported";
		default:
			return "Unknown";
	}
}

std::string lookup_ws_close_status_string(uint16_t code) {
	switch (code) {
		case 1000: 
			return "Normal closure";
		case 1001: 
			return "Going away";
		case 1002: 
			return "Protocol error";
		case 1003: 
			return "Unacceptable data";
		case 1004: 
			return "Reserved";
		case 1005: 
			return "No status received";
		case 1006: 
			return "Abnormal closure";
		case 1007: 
			return "Invalid message data";
		case 1008: 
			return "Policy Violation";
		case 1009: 
			return "Message too large";
		case 1010: 
			return "Missing required extensions";
		default:
			return "Unknown";
	}
}

bool websocketpp::ws_uri::parse(const std::string& uri) {
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
			port = (secure ? 443 : 80);
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
