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

#ifndef WEBSOCKETPP_TRANSPORT_BASE_CON_HPP
#define WEBSOCKETPP_TRANSPORT_BASE_CON_HPP

#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/functional.hpp>
#include <websocketpp/common/system_error.hpp>
#include <websocketpp/common/connection_hdl.hpp>

namespace websocketpp {
namespace transport {

/**
 * Transport needs to provide:
 * - async_read_at_least(size_t num_bytes, char *buf, size_t len, read_handler handler)
 *     start an async read for at least num_bytes and at most len bytes into buf. Call
 *     handler when done with number of bytes read. 
 *     
 *     WebSocket++ promises to have only one async_read_at_least in flight at a 
 *     time. The transport must promise to only call read_handler once per async
 *     read
 *     
 * - async_write(const char* buf, size_t len, write_handler handler)
 * - async_write(std::vector<buffer> & bufs, write_handler handler)
 *     start an async write of all of the data in buf or bufs. In second case data
 *     is written sequentially and in place without copying anything to a temporary
 *     location.
 *
 *     Websocket++ promises to have only one async_write in flight at a time.
 *     The transport must promise to only call the write_handler once per async
 *     write
 */

// Endpoint callbacks
typedef lib::function<void(connection_hdl,const lib::error_code&)> accept_handler;

typedef lib::function<void()> endpoint_lock;

// Connection callbacks
typedef lib::function<void(const lib::error_code&)> init_handler;
typedef lib::function<void(const lib::error_code&,size_t)> read_handler;
typedef lib::function<void(const lib::error_code&)> write_handler;
typedef lib::function<void()> inturrupt_handler;
typedef lib::function<void()> dispatch_handler;

typedef lib::function<void()> connection_lock;

struct buffer {
    buffer(const char * b, size_t l) : buf(b),len(l) {}

    const char* buf;
    size_t len;
};

} // namespace transport
} // namespace websocketpp

#endif // WEBSOCKETPP_TRANSPORT_BASE_CON_HPP
