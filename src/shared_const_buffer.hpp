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

#ifndef SHARED_CONST_BUFFER_HPP
#define SHARED_CONST_BUFFER_HPP


#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>

#include <string>
#include <vector>

namespace websocketpp {

class shared_const_buffer {
public:
    explicit shared_const_buffer(const std::string &data) : m_data(new std::vector<char>(data.begin(), data.end())),
    m_buffer(boost::asio::buffer(*m_data)) {}
public:
    typedef boost::asio::const_buffer value_type;
    typedef const boost::asio::const_buffer *const_iterator;
    const boost::asio::const_buffer *begin() const { return &m_buffer; }
    const boost::asio::const_buffer *end() const { return &m_buffer + 1; }
private:
    boost::shared_ptr< std::vector<char> > m_data;
    boost::asio::const_buffer m_buffer;
};

} // namespace websocketpp

#endif // SHARED_CONST_BUFFER_HPP
