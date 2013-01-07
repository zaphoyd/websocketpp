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

#ifndef WEBSOCKETPP_TRANSPORT_IOSTREAM_CON_HPP
#define WEBSOCKETPP_TRANSPORT_IOSTREAM_CON_HPP

#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/connection_hdl.hpp>

#include <websocketpp/transport/base/connection.hpp>
#include <websocketpp/transport/iostream/base.hpp>

#include <iostream>
#include <vector>

namespace websocketpp {
namespace transport {
namespace iostream {

template <typename concurrency>
class connection {
public:
    /// Type of this connection transport component
	typedef connection<concurrency> type;
    /// Type of a shared pointer to this connection transport component
    typedef lib::shared_ptr<type> ptr;
	
    // TODO: clean up the rest of these types
	class handler_interface {};
	
	typedef lib::shared_ptr<handler_interface> handler_ptr;
	
	typedef typename concurrency::scoped_lock_type scoped_lock_type;
	typedef typename concurrency::mutex_type mutex_type;
	
	explicit connection(bool is_server)
	  : m_reading(false)
	  , m_is_server(is_server)
	{
		std::cout << "iostream connection transport constructor" << std::endl;
	}
	
	void register_ostream(std::ostream* o) {
		// TODO: lock transport state?
		
		output_stream = o;
	}
	
	friend std::istream& operator>> (std::istream &in, type &t) {
	    // this serializes calls to external read.
		scoped_lock_type lock(t.m_read_mutex);
		
		t.read(in);
		
		return in;
	}
	
	bool is_secure() const {
	    return false;
	}
protected:
	void init(init_handler callback) {
		std::cout << "iostream connection init" << std::endl;
		callback(lib::error_code());
	}
	
	// post?
	
	
	/// read at least num_bytes bytes into buf and then call handler. 
	/**
	 * 
	 * 
	 */
	void async_read_at_least(size_t num_bytes, char *buf, size_t len, 
		read_handler handler)
	{
		std::cout << "iostream_con async_read_at_least: " << num_bytes << std::endl;
		
		if (num_bytes > len) {
		    handler(make_error_code(error::invalid_num_bytes),size_t(0));
		    return;
		}
		
		if (m_reading == true) {
		    handler(make_error_code(error::double_read),size_t(0));
		    return;
		}
		
		m_buf = buf;
		m_len = len;
		m_bytes_needed = num_bytes;
		m_read_handler = handler;
		m_cursor = 0;
		m_reading = true;
	}
	
	void async_write(const char* buf, size_t len, write_handler handler) {
	    std::cout << "iostream_con async_write" << std::endl;
		// TODO: lock transport state?
		
		if (!output_stream) {
			handler(make_error_code(error::output_stream_required));
			return;
		}
		
		output_stream->write(buf,len);
		
		if (output_stream->bad()) {
			handler(make_error_code(error::bad_stream));
		} else {
			handler(lib::error_code());
		}
	}
	
    /// Write
    /**
     * TODO: unit tests
     */
    void async_write(const std::vector<buffer>& bufs, write_handler handler) {
	    std::cout << "iostream_con async_write buffer list" << std::endl;
		// TODO: lock transport state?
		
		if (!output_stream) {
			handler(make_error_code(error::output_stream_required));
			return;
		}
        
        std::vector<buffer>::const_iterator it;
	    for	(it = bufs.begin(); it != bufs.end(); it++) {
            output_stream->write((*it).buf,(*it).len);
            
            if (output_stream->bad()) {
                handler(make_error_code(error::bad_stream));
            } else {
                handler(lib::error_code());
            }
        }
    }

	void set_handler(handler_ptr new_handler) {
		m_handler = new_handler;
	}
    
    /// Set Connection Handle
    /**
     * @param hdl The new handle
     */
    void set_handle(connection_hdl hdl) {
        m_connection_hdl = hdl;
    }

    lib::error_code dispatch(dispatch_handler handler) {
        handler(); 
        return lib::error_code();
    }

    void shutdown() {
        // TODO:
    }
private:
	void read(std::istream &in) {
	    std::cout << "iostream_con read" << std::endl;
		
		while (in.good()) {
		    if (!m_reading) {
		        std::cout << "write while not reading" << std::endl;
                break;
		    }
		    
            in.read(m_buf,m_len-m_cursor);
            
            if (in.gcount() == 0) {
                std::cout << "read zero bytes" << std::endl;
                break;
            }
            
            m_cursor += in.gcount();
            
            // TODO: error handling
            if (in.bad()) {
                m_reading = false;
                m_read_handler(make_error_code(error::bad_stream), m_cursor);
            }
            
            if (m_cursor >= m_bytes_needed) {
                m_reading = false;
                m_read_handler(lib::error_code(), m_cursor);
            }
        }
	}
	
	// Read space (Protected by m_read_mutex)
	char*			m_buf;
	size_t          m_len;
	size_t			m_bytes_needed;
	read_handler 	m_read_handler;
	size_t			m_cursor;
	
	// transport resources
	std::ostream*   output_stream;
	handler_ptr		m_handler;
    connection_hdl  m_connection_hdl;
	
	bool			m_reading;
	const bool		m_is_server;

	// This lock ensures that only one thread can edit read data for this 
	// connection. This is a very coarse lock that is basically locked all the 
	// time. The nature of the connection is such that it cannot be 
	// parallelized, the locking is here to prevent intra-connection concurrency
	// in order to allow inter-connection concurrency.
	mutex_type      m_read_mutex;
};


} // namespace iostream
} // namespace transport
} // namespace websocketpp

#endif // WEBSOCKETPP_TRANSPORT_IOSTREAM_CON_HPP
