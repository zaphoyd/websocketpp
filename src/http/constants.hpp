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

#ifndef HTTP_CONSTANTS_HPP
#define HTTP_CONSTANTS_HPP

namespace websocketpp {
namespace http {
    namespace status_code {
        enum value {
            CONTINUE = 100,
            SWITCHING_PROTOCOLS = 101,
            
            OK = 200,
            CREATED = 201,
            ACCEPTED = 202,
            NON_AUTHORITATIVE_INFORMATION = 203,
            NO_CONTENT = 204,
            RESET_CONTENT = 205,
            PARTIAL_CONTENT = 206,
            
            MULTIPLE_CHOICES = 300,
            MOVED_PERMANENTLY = 301,
            FOUND = 302,
            SEE_OTHER = 303,
            NOT_MODIFIED = 304,
            USE_PROXY = 305,
            TEMPORARY_REDIRECT = 307,
            
            BAD_REQUEST = 400,
            UNAUTHORIZED = 401,
            PAYMENT_REQUIRED = 402,
            FORBIDDEN = 403,
            NOT_FOUND = 404,
            METHOD_NOT_ALLOWED = 405,
            NOT_ACCEPTABLE = 406,
            PROXY_AUTHENTICATION_REQUIRED = 407,
            REQUEST_TIMEOUT = 408,
            CONFLICT = 409,
            GONE = 410,
            LENGTH_REQUIRED = 411,
            PRECONDITION_FAILED = 412,
            REQUEST_ENTITY_TOO_LARGE = 413,
            REQUEST_URI_TOO_LONG = 414,
            UNSUPPORTED_MEDIA_TYPE = 415,
            REQUEST_RANGE_NOT_SATISFIABLE = 416,
            EXPECTATION_FAILED = 417,
            IM_A_TEAPOT = 418,
            UPGRADE_REQUIRED = 426,
            PRECONDITION_REQUIRED = 428,
            TOO_MANY_REQUESTS = 429,
            REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
            
            INTERNAL_SERVER_ERROR = 500,
            NOT_IMPLIMENTED = 501,
            BAD_GATEWAY = 502,
            SERVICE_UNAVAILABLE = 503,
            GATEWAY_TIMEOUT = 504,
            HTTP_VERSION_NOT_SUPPORTED = 505,
            NOT_EXTENDED = 510,
            NETWORK_AUTHENTICATION_REQUIRED = 511
        };
        
        // TODO: should this be inline?
        inline std::string get_string(value c) {
            switch (c) {
                case CONTINUE:
                    return "Continue";
                case SWITCHING_PROTOCOLS:
                    return "Switching Protocols";
                case OK:
                    return "OK";
                case CREATED:
                    return "Created";
                case ACCEPTED:
                    return "Accepted";
                case NON_AUTHORITATIVE_INFORMATION:
                    return "Non Authoritative Information";
                case NO_CONTENT:
                    return "No Content";
                case RESET_CONTENT:
                    return "Reset Content";
                case PARTIAL_CONTENT:
                    return "Partial Content";
                case MULTIPLE_CHOICES:
                    return "Multiple Choices";
                case MOVED_PERMANENTLY:
                    return "Moved Permanently";
                case FOUND:
                    return "Found";
                case SEE_OTHER:
                    return "See Other";
                case NOT_MODIFIED:
                    return "Not Modified";
                case USE_PROXY:
                    return "Use Proxy";
                case TEMPORARY_REDIRECT:
                    return "Temporary Redirect";
                case BAD_REQUEST:
                    return "Bad Request";
                case UNAUTHORIZED:
                    return "Unauthorized";
                case FORBIDDEN:
                    return "Forbidden";
                case NOT_FOUND:
                    return "Not Found";
                case METHOD_NOT_ALLOWED:
                    return "Method Not Allowed";
                case NOT_ACCEPTABLE:
                    return "Not Acceptable";
                case PROXY_AUTHENTICATION_REQUIRED:
                    return "Proxy Authentication Required";
                case REQUEST_TIMEOUT:
                    return "Request Timeout";
                case CONFLICT:
                    return "Conflict";
                case GONE:
                    return "Gone";
                case LENGTH_REQUIRED:
                    return "Length Required";
                case PRECONDITION_FAILED:
                    return "Precondition Failed";
                case REQUEST_ENTITY_TOO_LARGE:
                    return "Request Entity Too Large";
                case REQUEST_URI_TOO_LONG:
                    return "Request-URI Too Long";
                case UNSUPPORTED_MEDIA_TYPE:
                    return "Unsupported Media Type";
                case REQUEST_RANGE_NOT_SATISFIABLE:
                    return "Requested Range Not Satisfiable";
                case EXPECTATION_FAILED:
                    return "Expectation Failed";
                case IM_A_TEAPOT:
                    return "I'm a teapot";
                case UPGRADE_REQUIRED:
                    return "Upgrade Required";
                case PRECONDITION_REQUIRED:
                    return "Precondition Required";
                case TOO_MANY_REQUESTS:
                    return "Too Many Requests";
                case REQUEST_HEADER_FIELDS_TOO_LARGE:
                    return "Request Header Fields Too Large";
                case INTERNAL_SERVER_ERROR:
                    return "Internal Server Error";
                case NOT_IMPLIMENTED:
                    return "Not Implimented";
                case BAD_GATEWAY:
                    return "Bad Gateway";
                case SERVICE_UNAVAILABLE:
                    return "Service Unavailable";
                case GATEWAY_TIMEOUT:
                    return "Gateway Timeout";
                case HTTP_VERSION_NOT_SUPPORTED:
                    return "HTTP Version Not Supported";
                case NOT_EXTENDED:
                    return "Not Extended";
                case NETWORK_AUTHENTICATION_REQUIRED:
                    return "Network Authentication Required";
                default:
                    return "Unknown";
            }
        }
    }
    
    class exception : public std::exception {
    public: 
        exception(const std::string& log_msg,
                  status_code::value error_code,
                  const std::string& error_msg = "",
                  const std::string& body = "")
        : m_msg(log_msg),
        m_error_code(error_code),
        m_error_msg(error_msg),
        m_body(body) {}
        ~exception() throw() {}
        
        virtual const char* what() const throw() {
            return m_msg.c_str();
        }
        
        std::string         m_msg;
        status_code::value  m_error_code;
        std::string         m_error_msg;
        std::string         m_body;
    };
}
}

#endif // HTTP_CONSTANTS_HPP
