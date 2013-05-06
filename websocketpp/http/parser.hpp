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

#ifndef HTTP_PARSER_HPP
#define HTTP_PARSER_HPP

#include <algorithm>
#include <iostream>
#include <map>

#include <websocketpp/http/constants.hpp>

namespace websocketpp {
namespace http {
namespace parser {

namespace state {
    enum value {
        method,
        resource,
        version,
        headers
    };
}

typedef std::map<std::string,std::string> header_list;

/// Read until a non-token character is found and then return the token and
/// iterator to the next character to read
template <typename InputIterator>
std::pair<std::string,InputIterator> extract_token(InputIterator begin,
    InputIterator end)
{
    InputIterator it = std::find_if(begin,end,&is_not_token_char);
    return std::make_pair(std::string(begin,it),it);
}

template <typename InputIterator>
std::pair<std::string,InputIterator> extract_quoted_string(InputIterator begin,
    InputIterator end)
{
    std::string s;
    
    if (end == begin) {
        return std::make_pair(s,begin);
    }

    if (*begin != '"') {
        return std::make_pair(s,begin);
    }

    InputIterator cursor = begin+1;
    InputIterator marker = cursor;

    cursor = std::find(cursor,end,'"');
    
    while (cursor != end) {
        // either this is the end or a quoted string
        if (*(cursor-1) == '\\') {
            s.append(marker,cursor-1);
            s.append(1,'"');
            ++cursor;
            marker = cursor;
        } else {
            s.append(marker,cursor);
            ++cursor;
            return std::make_pair(s,cursor);
        }
        
        cursor = std::find(cursor,end,'"');
    }

    return std::make_pair("",begin);
}

/// Read one unit of linear white space and return the iterator to the character
/// afterwards. If ret = begin no whitespace was extracted.
template <typename InputIterator>
InputIterator extract_lws(InputIterator begin, InputIterator end) {
    InputIterator it = begin;

    // strip leading CRLF
    if (end-begin > 2 && *begin == '\r' && *(begin+1) == '\n' && 
        is_whitespace_char(static_cast<unsigned char>(*(begin+2))))
    {
        it+=3;
    }

    it = std::find_if(it,end,&is_not_whitespace_char);
    return it;
}

/// SImilar to extract_lws but extracts all lws instead of just one line
template <typename InputIterator>
InputIterator extract_all_lws(InputIterator begin, InputIterator end) {
    InputIterator old_it;
    InputIterator new_it = begin;

    do {
        // Pull value from previous iteration
        old_it = new_it;
        
        // look ahead another pass
        new_it = extract_lws(old_it,end);
    } while (new_it != end && old_it != new_it);

    return new_it;
}

/*
struct attribute {
    attribute(const std::string &n, const std::string &v) 
      : name(n), value(v){}
    
    std::string name;
    std::string value;
};
typedef std::vector<attribute> attribute_list;

struct parameter {
    parameter(std::string n) : name(n) {}

    void add_attribute(const attribute& p) {
        attributes.push_back(p);
    }
    
    void add_attribute(const std::string& key, const std::string & value) {
        attributes.push_back(attribute(key,value));
    }
    
    std::string name; 
    attribute_list attributes;
};
typedef std::vector<parameter> parameter_list;
*/

//typedef std::map<std::string,std::string> string_map;
//typedef std::vector< std::pair< std::string, attribute_list > > parameter_list;

typedef std::map<std::string,std::string> attribute_list;
//typedef std::map<std::string,attribute_list> parameter_list;
typedef std::vector< std::pair< std::string, attribute_list > > parameter_list;

template <typename InputIterator>
InputIterator extract_attributes(InputIterator begin, InputIterator end,
    attribute_list & attributes)
{
    InputIterator cursor;
    bool first = true;

    if (begin == end) {
        return begin;
    }

    cursor = begin;
    std::pair<std::string,InputIterator> ret;

    while (cursor != end) {
        std::string name;
        
        cursor = http::parser::extract_all_lws(cursor,end);
        if (cursor == end) {
            break;
        }

        if (first) {
            // ignore this check for the very first pass
            first = false;  
        } else {
            if (*cursor == ';') {
                // advance past the ';'
                ++cursor;
            } else {
                // non-semicolon in this position indicates end end of the
                // attribute list, break and return.
                break;
            }
        }

        cursor = http::parser::extract_all_lws(cursor,end);
        ret = http::parser::extract_token(cursor,end);

        if (ret.first == "") {
            // error: expected a token
            return begin;
        } else {
            name = ret.first;
            cursor = ret.second;
        }

        cursor = http::parser::extract_all_lws(cursor,end);
        if (cursor == end || *cursor != '=') {
            // if there is an equals sign, read the attribute value. Otherwise
            // record a blank value and continue
            attributes[name] = "";
            continue;
        }
        
        // advance past the '='
        ++cursor;

        cursor = http::parser::extract_all_lws(cursor,end);
        if (cursor == end) {
            // error: expected a token or quoted string
            return begin;
        }
        
        ret = http::parser::extract_quoted_string(cursor,end);
        if (ret.second != cursor) {
            attributes[name] = ret.first;
            cursor = ret.second;
            continue;
        }

        ret = http::parser::extract_token(cursor,end);
        if (ret.first == "") {
            // error : expected token or quoted string
            return begin;
        } else {
            attributes[name] = ret.first;
            cursor = ret.second;
        }
    }
    
    return cursor;
}

template <typename InputIterator>
InputIterator extract_parameters(InputIterator begin, InputIterator end, 
    parameter_list &parameters) 
{
    InputIterator cursor;

    if (begin == end) {
        // error: expected non-zero length range
        return begin;
    }

    cursor = begin;
    std::pair<std::string,InputIterator> ret;
    
    /**
     * LWS
     * token
     * LWS
     * *(";" method-param)
     * LWS
     * ,=loop again
     */
    while (cursor != end) {
        std::string parameter_name;
        attribute_list attributes;

        // extract any stray whitespace
        cursor = http::parser::extract_all_lws(cursor,end);
        if (cursor == end) {break;}

        ret = http::parser::extract_token(cursor,end);

        if (ret.first == "") {
            // error: expected a token
            return begin;
        } else {
            parameter_name = ret.first;
            cursor = ret.second;
        }
        
        // Safe break point, insert parameter with blank attributes and exit
        cursor = http::parser::extract_all_lws(cursor,end);
        if (cursor == end) {
            //parameters[parameter_name] = attributes;
            parameters.push_back(std::make_pair(parameter_name,attributes));
            break;
        }

        // If there is an attribute list, read it in
        if (*cursor == ';') {
            InputIterator acursor;

            ++cursor;
            acursor = http::parser::extract_attributes(cursor,end,attributes);

            if (acursor == cursor) {
                // attribute extraction ended in syntax error
                return begin;
            }

            cursor = acursor;
        }
        
        // insert parameter into output list
        //parameters[parameter_name] = attributes;
        parameters.push_back(std::make_pair(parameter_name,attributes));
        
        cursor = http::parser::extract_all_lws(cursor,end);
        if (cursor == end) {break;}
        
        // if next char is ',' then read another parameter, else stop
        if (*cursor != ',') {
            break;
        }

        // advance past comma
        ++cursor;

        if (cursor == end) {
            // expected more bytes after a comma
            return begin;
        }
    }
    
    return cursor;
}

class parser {
public:
    typedef http::parser::attribute_list attribute_list;
    typedef http::parser::parameter_list parameter_list;
    
    // Convenience method versions of some of the free utility functions.
    bool parse_parameter_list(const std::string& in, parameter_list& out) const;

    /// Set the HTTP version string
    /**
     * @param version HTTP version string to use. Must be in format HTTP/x.y 
     *    where x and y are positive integers.
     */
    void set_version(const std::string& version);
    
    /// Get the HTTP version string
    const std::string& get_version() const {
        return m_version;
    }
    
    /// Get the HTTP header with name `key`
    /**
     * @param key Name of the header to return
     * @return Value of the header
     */
    const std::string& get_header(const std::string& key) const;
    
    /// Get the body string
    const std::string& get_body() const {
        return m_body;
    }
    
    /// Get the HTTP header with name `key`
    /**
     * 
     * @param key The header name to retrieve
     *
     * @param out A reference to a parameter list to store any extracted
     * paramters.
     *
     * @return True if the value of this header was not a valid parameter list
     */
    bool get_header_as_plist(const std::string& key, parameter_list& out) const;

    /// Append a header
    /**
     * If a header with this name already exists the value will be appended to
     * the existing header to form a comma separated list of values. Use
     * replace_header to overwrite existing values.
     * 
     * @param key Name of the header to set
     * @param val Value to add
     * @see replace_header
     */
    void append_header(const std::string &key,const std::string &val);
    
    /// Replace a header
    /**
     * If a header with this name already exists the old value will be replaced
     * Use add_header to append to a list of existing values.
     * 
     * @param key Name of the header to set
     * @param val Value to set
     * @see add_header
     */
    void replace_header(const std::string &key,const std::string &val);
    
    /// Remove a header
    void remove_header(const std::string &key);
    
    /// Set body content
    /**
     * Set the body content of the HTTP response to the parameter string. Note
     * set_body will also set the Content-Length HTTP header to the appropriate
     * value. If you want the Content-Length header to be something else, do so
     * via replace_header("Content-Length") after calling set_body()
     * 
     * @param value String data to include as the body content.
     */
    void set_body(const std::string& value);
protected:
    /// DEPRECATED Read headers out of an istream
    bool parse_headers(std::istream& s);
    
    /// Helper function for consume. Process header line
    void process_header(std::string::iterator begin, std::string::iterator end);
    
    /// Return headers in raw string form.
    std::string raw_headers() const;

    std::string m_version;
    header_list m_headers;
    std::string m_body;
};

} // namespace parser
} // namespace http
} // namespace websocketpp

#include <websocketpp/http/impl/parser.hpp>

#endif // HTTP_PARSER_HPP
