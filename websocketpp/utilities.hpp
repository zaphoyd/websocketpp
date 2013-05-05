/*
 * Copyright (c) 2012, Peter Thorson. All rights reserved.
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

#ifndef WEBSOCKETPP_UTILITIES_HPP
#define WEBSOCKETPP_UTILITIES_HPP

#include <algorithm>
#include <websocketpp/common/stdint.hpp>

namespace websocketpp {
namespace utility {

// case insensitive find
// http://stackoverflow.com/questions/3152241/case-insensitive-stdstring-find

// templated version of my_equal so it could work with both char and wchar_t
template<typename charT>
struct my_equal {
    my_equal( const std::locale& loc ) : loc_(loc) {}
    bool operator()(charT ch1, charT ch2) {
        return std::toupper(ch1, loc_) == std::toupper(ch2, loc_);
    }
private:
    const std::locale& loc_;
};

// find substring (case insensitive)
template<typename T>
typename T::const_iterator ci_find_substr( const T& str1, const T& str2, 
    const std::locale& loc = std::locale() )
{
    return std::search( str1.begin(), str1.end(), 
        str2.begin(), str2.end(), my_equal<typename T::value_type>(loc) );
}

template<typename T>
typename T::const_iterator ci_find_substr(const T& str1, 
    const typename T::value_type* str2, typename T::size_type size, 
    const std::locale& loc = std::locale())
{
    return std::search( str1.begin(), str1.end(), 
        str2, str2+size, my_equal<typename T::value_type>(loc) );
}



std::string string_replace_all(std::string subject, const std::string& search,
                          const std::string& replace);

/// Convert std::string to ascii printed string of hex digits
std::string to_hex(const std::string& input);

/// Convert c string to ascii printed string of hex digits
std::string to_hex(const uint8_t* input, size_t length);
std::string to_hex(const char* input, size_t length);

} // namespace utility
} // namespace websocketpp

#include <websocketpp/impl/utilities_impl.hpp>

#endif // WEBSOCKETPP_UTILITIES_HPP
