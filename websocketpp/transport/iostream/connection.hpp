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

#ifndef WEBSOCKETPP_TRANSPORT_IOSTREAM_CON_HPP
#define WEBSOCKETPP_TRANSPORT_IOSTREAM_CON_HPP

#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/logger/levels.hpp>

#include <websocketpp/transport/base/connection.hpp>
#include <websocketpp/transport/iostream/base.hpp>

#include <sstream>
#include <vector>

namespace websocketpp {
namespace transport {
namespace iostream {

/// Empty timer class to stub out for timer functionality that iostream
/// transport doesn't support
struct timer {
    void cancel() {}
};

template <typename config>
class connection {
public:
    /// Type of this connection transport component
    typedef connection<config> type;
    /// Type of a shared pointer to this connection transport component
    typedef lib::shared_ptr<type> ptr;

    /// transport concurrency policy
    typedef typename config::concurrency_type concurrency_type;
    /// Type of this transport's access logging policy
    typedef typename config::alog_type alog_type;
    /// Type of this transport's error logging policy
    typedef typename config::elog_type elog_type;

    // Concurrency policy types
    typedef typename concurrency_type::scoped_lock_type scoped_lock_type;
    typedef typename concurrency_type::mutex_type mutex_type;

    typedef lib::shared_ptr<timer> timer_ptr;

    explicit connection(bool is_server, alog_type& alog, elog_type& elog)
      : m_output_stream(NULL)
      , m_reading(false)
      , m_is_server(is_server)
      , m_alog(alog)
      , m_elog(elog)
    {
        m_alog.write(log::alevel::devel,"iostream con transport constructor");
    }

    /// Register a std::ostream with the transport for writing output
    /**
     * Register a std::ostream with the transport. All future writes will be
     * done to this output stream.
     *
     * @param o A pointer to the ostream to use for output.
     */
    void register_ostream(std::ostream* o) {
        // TODO: lock transport state?
        scoped_lock_type lock(m_read_mutex);
        m_output_stream = o;
    }

    /// Overloaded stream input operator
    /**
     * Attempts to read input from the given stream into the transport. Bytes
     * will be extracted from the input stream to fulfill any pending reads.
     * Input in this manner will only read until the current read buffer has
     * been filled. Then it will signal the library to process the input. If the
     * library's input handler adds a new async_read, additional bytes will be
     * read, otherwise the input operation will end.
     *
     * When this function returns one of the following conditions is true:
     * - There is no outstanding read operation
     * - There are no more bytes available in the input stream
     *
     * You can use tellg() on the input stream to determine if all of the input
     * bytes were read or not.
     *
     * If there is no pending read operation when the input method is called, it
     * will return immediately and tellg() will not have changed.
     */
    friend std::istream& operator>> (std::istream &in, type &t) {
        // this serializes calls to external read.
        scoped_lock_type lock(t.m_read_mutex);

        t.read(in);

        return in;
    }

    /// Manual input supply
    /**
     * Copies bytes from buf into WebSocket++'s input buffers. Bytes will be
     * copied from the supplied buffer to fulfill any pending library reads. It
     * will return the number of bytes successfully processed. If there are no
     * pending reads readsome will return immediately. Not all of the bytes may
     * be able to be read in one call
     */
    size_t readsome(const char *buf, size_t len) {
        // this serializes calls to external read.
        scoped_lock_type lock(m_read_mutex);

        return this->readsome_impl(buf,len);
    }


    /// Tests whether or not the underlying transport is secure
    /**
     * iostream transport will return false always because it has no information
     * about the ultimate remote endpoint. This may or may not be accurate
     * depending on the real source of bytes being input.
     *
     * TODO: allow user settable is_secure flag if this seems useful
     *
     * @return Whether or not the underlying transport is secure
     */
    bool is_secure() const {
        return false;
    }

    /// Get the remote endpoint address
    /**
     * The iostream transport has no information about the ultimate remote
     * endpoint. It will return the string "iostream transport". To indicate
     * this.
     *
     * TODO: allow user settable remote endpoint addresses if this seems useful
     *
     * @return A string identifying the address of the remote endpoint
     */
    std::string get_remote_endpoint() const {
        return "iostream transport";
    }

    /// Get the connection handle
    connection_hdl get_handle() const {
        return m_connection_hdl;
    }

    /// Call back a function after a period of time.
    /**
     * Timers are not implemented in this transport. The timer pointer will
     * always be empty. The handler will never be called.
     *
     * @param duration Length of time to wait in milliseconds
     *
     * @param callback The function to call back when the timer has expired
     *
     * @return A handle that can be used to cancel the timer if it is no longer
     * needed.
     */
    timer_ptr set_timer(long duration, timer_handler callback) {
        return timer_ptr();
    }
protected:
    void init(init_handler callback) {
        m_alog.write(log::alevel::devel,"iostream connection init");
        callback(lib::error_code());
    }

    /// Initiate an async_read for at least num_bytes bytes into buf
    /**
     * Initiates an async_read request for at least num_bytes bytes. The input
     * will be read into buf. A maximum of len bytes will be input. When the
     * operation is complete, handler will be called with the status and number
     * of bytes read.
     *
     * This method may or may not call handler from within the initial call. The
     * application should be prepared to accept either.
     *
     * The application should never call this method a second time before it has
     * been called back for the first read. If this is done, the second read
     * will be called back immediately with a double_read error.
     *
     * If num_bytes or len are zero handler will be called back immediately
     * indicating success.
     *
     * @param num_bytes Don't call handler until at least this many bytes have
     * been read.
     *
     * @param buf The buffer to read bytes into
     *
     * @param len The size of buf. At maximum, this many bytes will be read.
     *
     * @param handler The callback to invoke when the operation is complete or
     * ends in an error
     */
    void async_read_at_least(size_t num_bytes, char *buf, size_t len,
        read_handler handler)
    {
        std::stringstream s;
        s << "iostream_con async_read_at_least: " << num_bytes;
        m_alog.write(log::alevel::devel,s.str());

        if (num_bytes > len) {
            handler(make_error_code(error::invalid_num_bytes),size_t(0));
            return;
        }

        if (m_reading == true) {
            handler(make_error_code(error::double_read),size_t(0));
            return;
        }

        if (num_bytes == 0 || len == 0) {
            handler(lib::error_code(),size_t(0));
            return;
        }

        m_buf = buf;
        m_len = len;
        m_bytes_needed = num_bytes;
        m_read_handler = handler;
        m_cursor = 0;
        m_reading = true;
    }

    /// Asyncronous Transport Write
    /**
     * Write len bytes in buf to the output stream. Call handler to report
     * success or failure. handler may or may not be called during async_write,
     * but it must be safe for this to happen.
     *
     * Will return 0 on success. Other possible errors (not exhaustive)
     * output_stream_required: No output stream was registered to write to
     * bad_stream: a ostream pass through error
     *
     * @param buf buffer to read bytes from
     * @param len number of bytes to write
     * @param handler Callback to invoke with operation status.
     */
    void async_write(const char* buf, size_t len, write_handler handler) {
        m_alog.write(log::alevel::devel,"iostream_con async_write");
        // TODO: lock transport state?

        if (!m_output_stream) {
            handler(make_error_code(error::output_stream_required));
            return;
        }

        m_output_stream->write(buf,len);

        if (m_output_stream->bad()) {
            handler(make_error_code(error::bad_stream));
        } else {
            handler(lib::error_code());
        }
    }

    /// Asyncronous Transport Write (scatter-gather)
    /**
     * Write a sequence of buffers to the output stream. Call handler to report
     * success or failure. handler may or may not be called during async_write,
     * but it must be safe for this to happen.
     *
     * Will return 0 on success. Other possible errors (not exhaustive)
     * output_stream_required: No output stream was registered to write to
     * bad_stream: a ostream pass through error
     *
     * @param bufs vector of buffers to write
     * @param handler Callback to invoke with operation status.
     */
    void async_write(const std::vector<buffer>& bufs, write_handler handler) {
        m_alog.write(log::alevel::devel,"iostream_con async_write buffer list");
        // TODO: lock transport state?

        if (!m_output_stream) {
            handler(make_error_code(error::output_stream_required));
            return;
        }

        std::vector<buffer>::const_iterator it;
        for (it = bufs.begin(); it != bufs.end(); it++) {
            m_output_stream->write((*it).buf,(*it).len);

            if (m_output_stream->bad()) {
                handler(make_error_code(error::bad_stream));
            }
        }

        handler(lib::error_code());
    }

    /// Set Connection Handle
    /**
     * @param hdl The new handle
     */
    void set_handle(connection_hdl hdl) {
        m_connection_hdl = hdl;
    }

    /// Call given handler back within the transport's event system (if present)
    /**
     * Invoke a callback within the transport's event system if it has one. If
     * it doesn't, the handler will be invoked immediately before this function
     * returns.
     *
     * @param handler The callback to invoke
     *
     * @return Whether or not the transport was able to register the handler for
     * callback.
     */
    lib::error_code dispatch(dispatch_handler handler) {
        handler();
        return lib::error_code();
    }

    void async_shutdown(shutdown_handler h) {
        h(lib::error_code());
    }
private:
    void read(std::istream &in) {
        m_alog.write(log::alevel::devel,"iostream_con read");

        while (in.good()) {
            if (!m_reading) {
                m_elog.write(log::elevel::devel,"write while not reading");
                break;
            }

            in.read(m_buf+m_cursor,static_cast<std::streamsize>(m_len-m_cursor));

            if (in.gcount() == 0) {
                m_elog.write(log::elevel::devel,"read zero bytes");
                break;
            }

            m_cursor += static_cast<size_t>(in.gcount());

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

    size_t readsome_impl(const char * buf, size_t len) {
        m_alog.write(log::alevel::devel,"iostream_con readsome");

        if (!m_reading) {
            m_elog.write(log::elevel::devel,"write while not reading");
            return 0;
        }

        size_t bytes_to_copy = std::min(len,m_len-m_cursor);

        std::copy(buf,buf+bytes_to_copy,m_buf);

        m_cursor += bytes_to_copy;

        if (m_cursor >= m_bytes_needed) {
            m_reading = false;
            m_read_handler(lib::error_code(), m_cursor);
        }

        return bytes_to_copy;
    }

    // Read space (Protected by m_read_mutex)
    char*           m_buf;
    size_t          m_len;
    size_t          m_bytes_needed;
    read_handler    m_read_handler;
    size_t          m_cursor;

    // transport resources
    std::ostream*   m_output_stream;
    connection_hdl  m_connection_hdl;

    bool            m_reading;
    const bool      m_is_server;
    alog_type& m_alog;
    elog_type& m_elog;

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
