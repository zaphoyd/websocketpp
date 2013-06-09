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

#ifndef HTTP_PARSER_IMPL_HPP
#define HTTP_PARSER_IMPL_HPP

#include <algorithm>
#include <sstream>

namespace websocketpp {
namespace http {
namespace parser {

/// Extract an HTTP parameter list from a string.
/**
 * @param [in] in The input string.
 *
 * @param [out] out The parameter list to store extracted parameters in.
 *
 * @return Whether or not the input was a valid parameter list.
 */
inline bool parser::parse_parameter_list(std::string const & in, 
    parameter_list & out) const
{
    if (in.size() == 0) {
        return false;
    }
    
    std::string::const_iterator it;
    it = extract_parameters(in.begin(),in.end(),out);
    return (it == in.begin());
}

/// Extract an HTTP parameter list from a parser header.
/**
 * If the header requested doesn't exist or exists and is empty the parameter
 * list is valid (but empty).
 *
 * @param [in] key The name/key of the HTTP header to use as input.
 *
 * @param [out] out The parameter list to store extracted parameters in.
 *
 * @return Whether or not the input was a valid parameter list.
 */
inline bool parser::get_header_as_plist(std::string const & key, 
    parameter_list & out) const
{
    header_list::const_iterator it = m_headers.find(key);
    
    if (it == m_headers.end() || it->second.size() == 0) {
        return false;
    }

    return this->parse_parameter_list(it->second,out);
}

/// Set HTTP parser Version
/**
 * \todo Does this method need any validation?
 *
 * @param [in] version The value to set the HTTP version to.
 */
inline void parser::set_version(std::string const & version) {
    m_version = version;
}

/// Get the value of an HTTP header
/**
 * \todo Make this method case insensitive.
 *
 * @param [in] key The name/key of the header to get.
 *
 * @return The value associated with the given HTTP header key.
 */
inline std::string const & parser::get_header(std::string const & key) const {
    header_list::const_iterator h = m_headers.find(key);
    
    if (h == m_headers.end()) {
        return empty_header;
    } else {
        return h->second;
    }
}

/// Append a value to an existing HTTP header
/**
 * This method will set the value of the HTTP header `key` with the indicated 
 * value. If a header with the name `key` already exists, `val` will be appended
 * to the existing value.
 *
 * \todo Make this method case insensitive.
 * \todo Should there be any restrictions on which keys are allowed to be set?
 * \todo Exception free varient
 *
 * @param [in] key The name/key of the header to append to.
 *
 * @param [in] val The value to append.
 */
inline void parser::append_header(std::string const & key, std::string const &
    val)
{
    if (std::find_if(key.begin(),key.end(),is_not_token_char) != key.end()) {
        throw exception("Invalid header name",status_code::bad_request);
    }
    
    if (this->get_header(key) == "") {
        m_headers[key] = val;
    } else {
        m_headers[key] += ", " + val;
    }
}

/// Set a value for an HTTP header, replacing an existing value
/**
 * This method will set the value of the HTTP header `key` with the indicated 
 * value. If a header with the name `key` already exists, `val` will replace the
 * existing value.
 *
 * \todo Make this method case insensitive.
 * \todo Should there be any restrictions on which keys are allowed to be set?
 * \todo Exception free varient
 *
 * @param [in] key The name/key of the header to append to.
 *
 * @param [in] val The value to append.
 */
inline void parser::replace_header(std::string const & key, std::string const &
    val)
{
    m_headers[key] = val;
}

/// Remove a header from the parser
/**
 * Removes the header entirely from the parser. This is different than setting
 * the value of the header to blank.
 *
 * \todo Make this method case insensitive.
 *
 * @param [in] key The name/key of the header to remove.
 */
inline void parser::remove_header(std::string const & key) {
    m_headers.erase(key);
}

/// Set HTTP body
/**
 * Sets the body of the HTTP object and fills in the appropriate content length
 * header
 *
 * @param [in] value The value to set the body to.
 */
inline void parser::set_body(std::string const & value) {
    if (value.size() == 0) {
        remove_header("Content-Length");
        m_body = "";
        return;
    }
    
    std::stringstream len;
    len << value.size();
    replace_header("Content-Length", len.str());
    m_body = value;
}

/// Parse headers from an istream
/**
 * @param [in] s The istream to extract headers from.
 */
inline bool parser::parse_headers(std::istream& s) {
    std::string header;
    std::string::size_type end;
    
    // get headers
    while (std::getline(s, header) && header != "\r") {
        if (header[header.size()-1] != '\r') {
            continue; // ignore malformed header lines?
        } else {
            header.erase(header.end()-1);
        }
        
        end = header.find(header_separator,0);
        
        if (end != std::string::npos) {         
            append_header(header.substr(0,end),header.substr(end+2));
        }
    }
    
    return true;
}

/// Generate and return the HTTP headers as a string
/**
 * Each headers will be followed by the \r\n sequence including the last one.
 * A second \r\n sequence (blank header) is not appended by this method
 *
 * @return The HTTP headers as a string.
 */
inline std::string parser::raw_headers() const {
    std::stringstream raw;
    
    header_list::const_iterator it;
    for (it = m_headers.begin(); it != m_headers.end(); it++) {
        raw << it->first << ": " << it->second << "\r\n";
    }
    
    return raw.str();
}

/// Process a header
/**
 *
 * \todo Update this method to be exception free.
 *
 * @param [in] s The istream to extract headers from.
 */
inline void parser::process_header(std::string::iterator begin, 
    std::string::iterator end)
{
    std::string::iterator cursor = std::search(
        begin,
        end,
        header_separator,
        header_separator + sizeof(header_separator) - 1
    );
    
    if (cursor == end) {
        throw exception("Invalid header line",status_code::bad_request);
    }
    
    append_header(std::string(begin,cursor),
                  std::string(cursor+sizeof(header_separator)-1,end));
}

} // namespace parser
} // namespace http
} // namespace websocketpp

#endif // HTTP_PARSER_IMPL_HPP
