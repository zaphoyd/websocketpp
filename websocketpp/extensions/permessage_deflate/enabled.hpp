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

#ifndef WEBSOCKETPP_PROCESSOR_EXTENSION_PERMESSAGEDEFLATE_HPP
#define WEBSOCKETPP_PROCESSOR_EXTENSION_PERMESSAGEDEFLATE_HPP

#include <websocketpp/common/cpp11.hpp>
#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/system_error.hpp>
#include <websocketpp/error.hpp>

#include <websocketpp/extensions/extension.hpp>

#include "zlib.h"

#include <string>
#include <vector>

namespace websocketpp {
namespace extensions {

/// Implimentation of the draft permessage-deflate WebSocket extension
/**
 * ### permessage-deflate interface
 *
 * **is_implimented**\n
 * `bool is_implimented()`\n
 * Returns whether or not the object impliments the extension or not
 *
 * **is_enabled**\n
 * `bool is_enabled()`\n
 * Returns whether or not the extension was negotiated for the current 
 * connection
 *
 * **negotiate**\n
 * `err_str_pair negotiate(http::attribute_list const & attributes)`\n
 * Negotiate the parameters of extension use
 *
 * **compress**\n
 * `lib::error_code compress(std::string const & in, std::string & out)`\n
 * Compress the bytes in `in` and append them to `out`
 *
 * **decompress**\n
 * `lib::error_code decompress(uint8_t const * buf, size_t len, std::string & 
 * out)`\n
 * Decompress `len` bytes from `buf` and append them to string `out`
 */
namespace permessage_deflate {

/// Permessage deflate error values
namespace error {
enum value {
    /// Catch all
    general = 1,

    /// Invalid extension parameters
    invalid_parameters,

    /// Unsupported compression algorithm
    unsupported_algorithm,
    
    /// Unknown method parameter
    unknown_method_parameter,

    /// Invalid Algorithm Settings
    invalid_algorithm_settings,

    /// ZLib Error
    zlib_error,

    /// Uninitialized
    uninitialized,
};

/// Permessage-deflate error category
class category : public lib::error_category {
public:
    category() {}

    char const * name() const _WEBSOCKETPP_NOEXCEPT_TOKEN_ {
        return "websocketpp.extension.permessage-deflate";
    }

    std::string message(int value) const {
        switch(value) {
            case general:
                return "Generic permessage-compress error";
            case invalid_parameters:
                return "Invalid extension parameters";
            case unsupported_algorithm:
                return "Unsupported algorithm";
            case unknown_method_parameter:
                return "Unknown method parameter";
            case invalid_algorithm_settings:
                return "invalid algorithm settings";
            case zlib_error:
                return "A zlib function returned an error";
            case uninitialized:
                return "Object must be initialized before use";
            default:
                return "Unknown permessage-compress error";
        }
    }
};

/// Get a reference to a static copy of the permessage-deflate error category
lib::error_category const & get_category() {
    static category instance;
    return instance;
}

/// Create an error code in the permessage-deflate category
lib::error_code make_error_code(error::value e) {
    return lib::error_code(static_cast<int>(e), get_category());
}

} // namespace error
} // namespace permessage_deflate
} // namespace extensions
} // namespace websocketpp

_WEBSOCKETPP_ERROR_CODE_ENUM_NS_START_
template<> struct is_error_code_enum
    <websocketpp::extensions::permessage_deflate::error::value>
{
    static bool const value = true;
};
_WEBSOCKETPP_ERROR_CODE_ENUM_NS_END_
namespace websocketpp {
namespace extensions {
namespace permessage_deflate {

template <typename config>
class enabled {
public:
    enabled() 
      : m_enabled(false)
      , m_c2s_no_context_takeover(false)
      , m_s2c_no_context_takeover(false)
      , m_c2s_max_window_bits(15)
      , m_s2c_max_window_bits(15) {}

    /// Test if this object impliments the permessage-deflate specification
    /**
     * Because this object does impliment it, it will always return true.
     *
     * @return Whether or not this object impliments permessage-deflate
     */
    bool is_implemented() const {
        return true;
    }

    /// Test if the extension was negotiated for this connection
    /**
     * Retrieves whether or not this extension is in use based on the initial
     * handshake extension negotiations.
     *
     * @return Whether or not the extension is in use
     */
    bool is_enabled() const {
        return m_enabled;
    }
    
    /// Does this extension support resetting its sliding window?
    bool no_context_takeover_support() const {
        return false;
    }

    /// Does this extension support adjusting its sliding window size
    bool max_window_bits_support() const {
        return false;
    }

    /// Negotiate extension
    /**
     * Negotiate whether or not to use and parameter values for the extension
     * based on the request from the the remote endpoint and the local policy
     *
     * @param attributes Attribute from remote endpoint's request
     * @return Status code and value to return to remote endpoint
     */
    err_str_pair negotiate(http::attribute_list const & attributes) {
        std::string neg = "permessage-deflate";
        //return make_pair(make_error_code(error::zlib_error),std::string());
        return make_pair(lib::error_code(),neg);
    }

    /// Compress bytes
    /**
     * @param [in] in String to compress
     * @param [out] out String to append compressed bytes to
     * @return Error or status code
     */
    lib::error_code compress(std::string const & in, std::string & out) {
        return make_error_code(error::uninitialized);
    }

    /// Decompress bytes
    /**
     * @param buf Byte buffer to decompress
     * @param len Length of buf
     * @param out String to append decompressed bytes to
     * @return Error or status code
     */
    lib::error_code decompress(uint8_t const * buf, size_t len, std::string &
        out)
    {
        return make_error_code(error::uninitialized);
    }
private:
    bool m_enabled;
    bool m_c2s_no_context_takeover;
    bool m_s2c_no_context_takeover;
    uint8_t m_c2s_max_window_bits;
    uint8_t m_s2c_max_window_bits;
};

} // namespace permessage_deflate
} // namespace extensions
} // namespace websocketpp

#endif // WEBSOCKETPP_PROCESSOR_EXTENSION_PERMESSAGEDEFLATE_HPP
