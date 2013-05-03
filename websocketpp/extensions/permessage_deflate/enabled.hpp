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
#include <websocketpp/common/system_error.hpp>
#include <websocketpp/common/memory.hpp>

#include <websocketpp/extensions/extension.hpp>

#include "zlib.h"

#include <string>
#include <vector>

namespace websocketpp {
namespace extensions {
namespace permessage_deflate {
    
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

class category : public lib::error_category {
public:
    category() {}

    const char *name() const _WEBSOCKETPP_NOEXCEPT_TOKEN_ {
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

const lib::error_category& get_category() {
    static category instance;
    return instance;
}

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
    static const bool value = true;
};
_WEBSOCKETPP_ERROR_CODE_ENUM_NS_END_
namespace websocketpp {
namespace extensions {
namespace permessage_deflate {

template <typename config>
class method {
public:
    typedef typename config::request_type::attribute_list attribute_list;

    virtual const char* name() const = 0;
    virtual lib::error_code init(const attribute_list& attributes) = 0;
    virtual lib::error_code compress(const std::string& in, std::string& out);
    virtual lib::error_code decompress(const uint8_t * buf, size_t len, 
        std::string& out);
    virtual lib::error_code decompress(const std::string& in, std::string& out);
};

template <typename config>
class deflate_method : public method<config> {
public:
    typedef method<config> base;
    typedef typename base::attribute_list attribute_list;

    deflate_method(bool is_server) 
      : m_is_server(is_server)
      , s2c_no_context_takeover(false)
      , c2s_no_context_takeover(false)
      , s2c_max_window_bits(15) 
      , c2s_max_window_bits(15) {}

    const char* name() const {
        return "deflate";
    }
    
    lib::error_code init(const attribute_list& attributes) {
        typename attribute_list::const_iterator it;

        for (it = attributes.begin(); it != attributes.end(); ++it) {
            if (it->first == "s2c_no_context_takeover") {
                s2c_no_context_takeover = true;
            } else if (it->first == "c2s_no_context_takeover") {
                c2s_no_context_takeover = true;
            } else if (it->first == "s2c_max_window_bits") {
                std::istringstream ss(it->second);
                int temp;
                if ((ss >> temp).fail() || temp > 15 || temp < 8) {
                    return make_error_code(error::invalid_algorithm_settings);
                }
                s2c_max_window_bits = temp;
            } else if (it->first == "c2s_max_window_bits") {
                if (m_is_server) {
                    // we are allowed to control the max window size of the 
                    // client by sending back this parameter with a value.
                } else {
                    // The server requested that we limit our outgoing window
                    // size
                    std::istringstream ss(it->second);
                    int temp;
                    if ((ss >> temp).fail() || temp > 15 || temp < 8) {
                        return make_error_code(error::invalid_algorithm_settings);
                    }
                    c2s_max_window_bits = temp;
                }

            } else {
                return make_error_code(error::unknown_method_parameter);
            }
        }
        
        // initialize zlib
        m_zstream.zalloc = Z_NULL;
        m_zstream.zfree = Z_NULL;
        m_zstream.opaque = Z_NULL;

        int ret = deflateInit2(
            &m_zstream,
            // This is a CPU vs space tradeoff, TODO: make configurable
            Z_DEFAULT_COMPRESSION,
            Z_DEFLATED,
            // A smaller value here may be chosen to reduce memory usage 
            (m_is_server ? s2c_max_window_bits : c2s_max_window_bits),
            // memLevel (1-9), TODO: make configurable
            8,
            // strategy. maybe make configurable?
            Z_DEFAULT_STRATEGY
        );

        if (ret != Z_OK) {
            return make_error_code(error::zlib_error);
        }

        return lib::error_code();
    }

private:
    const bool m_is_server;

    bool s2c_no_context_takeover;
    bool c2s_no_context_takeover;
    uint8_t s2c_max_window_bits;
    uint8_t c2s_max_window_bits;
    

    z_stream m_zstream;
};


class deflate_engine {
public:
    deflate_engine()
      : m_valid(false)
    {
        m_dstate.zalloc = Z_NULL;
        m_dstate.zfree = Z_NULL;
        m_dstate.opaque = Z_NULL;

        m_istate.zalloc = Z_NULL;
        m_istate.zfree = Z_NULL;
        m_istate.opaque = Z_NULL;
        m_istate.avail_in = 0;
        m_istate.next_in = Z_NULL;
    }
    
    ~deflate_engine() {
        if (!m_valid) {
            // init was never successfully called, nothing to clean up.
            return;
        }

        int ret = deflateEnd(&m_dstate);

        if (ret != Z_OK) {
            std::cout << "error cleaning up zlib compression state" 
                      << std::endl;
        }

        ret = inflateEnd(&m_istate);

        if (ret != Z_OK) {
            std::cout << "error cleaning up zlib decompression state" 
                      << std::endl;
        }
    }
    
    // TODO: delete copy constructor and assignment operator

    /// Initialize zlib state
    /**
     * @param window_bits Sliding window size. Value range from 8-15. Higher 
     * values use more memory but provide better compression.
     *
     * @param compress_level How much compression to apply. Values range from 0
     * to 9. 1 is fastest, 9 is best compression. 0 provides no compression. The
     * default is ~6.
     *
     * @param mem_level How much memory to use for internal compression state
     * Values range from 1 to 9. 1 is lowest memory, 9 is best compression. 
     * Default is 8.
     *
     * @param strategy Passes through strategy tuning parameter. See zlib 
     * documentation for more information.
     *
     * @return A status code. 0 on success, non-zero otherwise.
     */
    lib::error_code init(
        int compress_window_bits = 15, int decompress_window_bits = 15,
        bool reset_compress = false, bool reset_decompress = false, 
        int compress_level = Z_DEFAULT_COMPRESSION, int mem_level = 8, 
        int strategy = Z_DEFAULT_STRATEGY, size_t compress_buffer = 16384)
    {
        int ret = deflateInit2(
            &m_dstate,
            compress_level,
            Z_DEFLATED,
            -1*compress_window_bits,
            mem_level,
            strategy
        );

        if (ret != Z_OK) {
            return make_error_code(error::zlib_error);
        }
        
        ret = inflateInit2(
            &m_istate,
            -1*decompress_window_bits
        );

        if (ret != Z_OK) {
            return make_error_code(error::zlib_error);
        }

        m_compress_buffer.reset(new unsigned char[compress_buffer]);
        m_compress_buffer_size = compress_buffer;

        m_valid = true;
        return lib::error_code();
    }
    
    /// Compress a string in one chunk
    /**
     *
     * Bytes input must be unmasked, output bytes are also unmasked.
     *
     * TODO: potential optimization to use copy+mask rather than copy
     *       is this a bad idea? probably.
     *
     */
    lib::error_code compress(const std::string& in, std::string& out) {
        if (!m_valid) {
            return make_error_code(error::uninitialized);
        }
        
        size_t output;
        int ret;

        // Read from input string
        m_dstate.avail_in = in.size();
        m_dstate.next_in = (unsigned char *)(const_cast<char *>(in.data()));

        do {
            // Output to local buffer
            m_dstate.avail_out = m_compress_buffer_size;
            m_dstate.next_out = m_compress_buffer.get();

            ret = deflate(&m_dstate,Z_SYNC_FLUSH);
            // assert(ret != Z_STREAM_ERROR);
            //
            output = m_compress_buffer_size - m_dstate.avail_out;

            out.append((char *)(m_compress_buffer.get()),output);
        } while (m_dstate.avail_out == 0);
        // assert(m_zlib_state.avail_in == 0); // all input should be used

        return lib::error_code();
    }
    
    lib::error_code decompress(const std::string& in, std::string& out) {
        return this->decompress(
            reinterpret_cast<const uint8_t *>(in.data()),
            in.size(),
            out
        );
    }

    lib::error_code decompress(const uint8_t * buf, size_t len, 
        std::string & out)
    {
        if (!m_valid) {
            return make_error_code(error::uninitialized);
        }
        
        int ret;

        m_istate.avail_in = len;
        m_istate.next_in = const_cast<unsigned char *>(buf);
        
        do {
            m_istate.avail_out = m_compress_buffer_size;
            m_istate.next_out = m_compress_buffer.get();

            ret = inflate(&m_istate,Z_SYNC_FLUSH);
            
            if (ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
                return make_error_code(error::zlib_error);
            }

            out.append(
                reinterpret_cast<char *>(m_compress_buffer.get()),
                m_compress_buffer_size - m_istate.avail_out
            );
        } while (m_istate.avail_out == 0);

        return lib::error_code();
    }
private:
    bool m_valid;
    
    lib::unique_ptr_uchar_array m_compress_buffer;

    size_t  m_compress_buffer_size;

    z_stream m_dstate; // deflate state
    z_stream m_istate; // inflate state
};

/// Impliments the permessage_deflate extension interface
/**
 * 
 * Template parameter econfig defines compile time types, constants, and 
 * settings. It should be a struct with the following members:
 *
 * request_type (type) A type that impliments the http::request interface
 * 
 * allow_disabling_context_takeover (static const bool) whether or not to
 * disable context takeover when the other endpoint requests it.
 * 
 * 
 * Parses the attribute list and determines if there are any errors based on
 * permessage_compress spec. If there are, init will fail. init
 * 
 *
 *
 * Methods:
 * permessage_deflate::enabled does not define or impliment any methods 
 * itself. It uses the attribute list to determine
 *
 * 
 *
 *
 */
template <typename econfig>
class enabled {
    typedef std::map<std::string,std::string> string_map;

    typedef typename econfig::request_type::attribute_list attribute_list;
    typedef typename attribute_list::const_iterator attribute_iterator;
    typedef lib::shared_ptr< method<econfig> > method_ptr;
    
    typedef typename econfig::request_type request_type;
    
    typedef std::pair<lib::error_code,std::string> err_str_pair;

public:
    enabled() : m_enabled(false) {}
    
    /// Attempt to negotiate the permessage_deflate extension
    /**
     * Parses the attribute list for this extension and attempts to negotiate
     * the extension. Returns a pair<lib::error_code, std::string>. On success
     * the error code is zero and the string contains the negotated parameters
     * to return in the handshake response. On error
     *
     * @param attributes A list of attributes extracted from the 
     * 'permessage_deflate' extension parameter from the original handshake.
     * 
     * @return A pair<lib::error_code, std::string> containing a status code
     * and a value whose interpretation is dependent on the status code.
     */
    err_str_pair negotiate(const string_map& attributes) {
        err_str_pair ret;
        
        std::cout << "foo: " << attributes.size() << std::endl;

        string_map::const_iterator it;
        
        for (it = attributes.begin(); it != attributes.end(); ++it) {
            std::cout << it->first << ": " << it->second << std::endl;
        }
        
        // start by not accepting any parameters
        if (attributes.size() == 0) {
            ret.second = "permessage-deflate";
        } else {
            ret.first = make_error_code(error::invalid_parameters);
        }

        /*
         *
         * Sec-WebSocket-Extensions: foo; bar; baz=5, foo2; bar2; baz2=5
         * 
         * map<string,map<string,string>>
         * 
         * vector<pair<string,vector<pair<string,string>>>>
         *
         */

        
        /*
        // Exactly one parameter is required
        if (attributes.size() != 1) {
            ret.first = make_error_code(error::invalid_parameters);
            return ret;
        }
        
        attribute_iterator method = attributes.find("method");
        
        // That one parameter must be 'method'
        if (method == attributes.end()) {
            ret.first = make_error_code(error::invalid_parameters);
            return ret;
        }
        
        // It's value must be a valid parameter list
        request_type parameter_parser;
        typename request_type::parameter_list plist;
        
        if (parameter_parser.parse_parameter_list(method->second,plist)) {
            ret.first = make_error_code(error::invalid_parameters);
            return ret;
        }

        // This represents a list of potential parameter sets that we could use
        // in order of client preference. We need to validate the options to
        // make sure none are illegal. Then one of these needs to be chosen as
        // the actual value set to be used for the connection.
        
        typename request_type::parameter_list::const_iterator it;

        for (it = plist.begin(); it != plist.end(); ++it)
        {
            if (it->first == "deflate") {
                // we can do deflate
                //method_ptr new_method(new deflate_method<econfig>(true));

                //new_method->init(it->second);
            } else {
                // unsupported algorithm, ignore
            }
        }
        
        m_enabled = true;*/
        return ret;
    }
    
    /// Returns true if this object impliments permessage_deflate functionality
    bool is_implimented() const {
        return true;
    }
    
    /// returns true if this object is initialized and ready to provide 
    /// permessage_deflate functionality.
    bool is_enabled() const {
        return m_enabled;
    }

    lib::error_code compress(const std::string in, std::string &out) {
        if (!m_enabled) {
            return make_error_code(error::uninitialized);
        }
        return m_method->compress(in,out);
    }

    lib::error_code decompress(const uint8_t * buf, size_t len, 
        std::string &out)
    {
        if (!m_enabled) {
            return make_error_code(error::uninitialized);
        }
        return m_method->decompress(buf,len,out);
    }

    lib::error_code decompress(const std::string in, std::string &out) {
        if (!m_enabled) {
            return make_error_code(error::uninitialized);
        }
        return m_method->decompress(in,out);
    }
private:
    bool m_enabled;
    method_ptr m_method;
};

} // namespace permessage_deflate
} // namespace extensions
} // namespace websocketpp

#endif // WEBSOCKETPP_PROCESSOR_EXTENSION_PERMESSAGEDEFLATE_HPP
