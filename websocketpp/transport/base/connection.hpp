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

#ifndef WEBSOCKETPP_TRANSPORT_BASE_CON_HPP
#define WEBSOCKETPP_TRANSPORT_BASE_CON_HPP

#include <websocketpp/common/cpp11.hpp>
#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/common/functional.hpp>
#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/system_error.hpp>

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
 * 
 * - std::string remote_endpoint(): retrieve address of remote endpoint
 * - is_secure(): whether or not the connection to the remote endpoint is secure
 * - lib::error_code dispatch(dispatch_handler handler): invoke handler within
 *     the transport's event system if it uses one.
 */



// Connection callbacks
typedef lib::function<void(const lib::error_code&)> init_handler;
typedef lib::function<void(const lib::error_code&,size_t)> read_handler;
typedef lib::function<void(const lib::error_code&)> write_handler;
typedef lib::function<void(const lib::error_code&)> timer_handler;
typedef lib::function<void(const lib::error_code&)> shutdown_handler;
typedef lib::function<void()> inturrupt_handler;
typedef lib::function<void()> dispatch_handler;

typedef lib::function<void()> connection_lock;

struct buffer {
    buffer(const char * b, size_t l) : buf(b),len(l) {}

    const char* buf;
    size_t len;
};

namespace error {
enum value {
    /// Catch-all error for transport policy errors that don't fit in other
    /// categories
    general = 1,
    
    /// underlying transport pass through
    pass_through,
    
    /// async_read_at_least call requested more bytes than buffer can store
    invalid_num_bytes,
    
    /// async_read called while another async_read was in progress
    double_read,

    /// Operation aborted
    operation_aborted,
    
    /// Operation not supported
    operation_not_supported,
    
    /// End of file
    eof,
    
    /// TLS short read
    tls_short_read,
    
    /// Timer expired
    timeout
};

class category : public lib::error_category {
    public:
    category() {}

    const char *name() const _WEBSOCKETPP_NOEXCEPT_TOKEN_ {
        return "websocketpp.transport";
    }
    
    std::string message(int value) const {
        switch(value) {
            case general:
                return "Generic transport policy error";
            case pass_through:
                return "Underlying Transport Error";
            case invalid_num_bytes:
                return "async_read_at_least call requested more bytes than buffer can store";
            case operation_aborted:
                return "The operation was aborted";
            case operation_not_supported:
                return "The operation is not supported by this transport";
            case eof:
                return "End of File";
        	case tls_short_read:
                return "TLS Short Read";
            case timeout:
                return "Timer Expired";
            default:
                return "Unknown";
        }
    }
};

inline const lib::error_category& get_category() {
    static category instance;
    return instance;
}

inline lib::error_code make_error_code(error::value e) {
    return lib::error_code(static_cast<int>(e), get_category());
}

} // namespace error
} // namespace transport
} // namespace websocketpp
_WEBSOCKETPP_ERROR_CODE_ENUM_NS_START_
template<> struct is_error_code_enum<websocketpp::transport::error::value>
{
    static const bool value = true;
};
_WEBSOCKETPP_ERROR_CODE_ENUM_NS_END_

#endif // WEBSOCKETPP_TRANSPORT_BASE_CON_HPP
