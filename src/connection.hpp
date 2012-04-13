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

#ifndef WEBSOCKETPP_CONNECTION_HPP
#define WEBSOCKETPP_CONNECTION_HPP

#include "common.hpp"
#include "http/parser.hpp"
#include "logger/logger.hpp"

#include "roles/server.hpp"

#include "processors/hybi.hpp"

#include "messages/data.hpp"
#include "messages/control.hpp"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <iostream> // temporary?
#include <vector>
#include <string>
#include <queue>
#include <set>

#ifdef min
	#undef min
#endif // #ifdef min

namespace websocketpp {

class endpoint_base;

template <typename T>
struct connection_traits;
    
template <
    typename endpoint,
    template <class> class role,
    template <class> class socket>
class connection 
 : public role< connection<endpoint,role,socket> >,
   public socket< connection<endpoint,role,socket> >,
   public boost::enable_shared_from_this< connection<endpoint,role,socket> >
{
public:
    typedef connection_traits< connection<endpoint,role,socket> > traits;
    
    // get types that we need from our traits class
    typedef typename traits::type type;
    typedef typename traits::ptr ptr;
    typedef typename traits::role_type role_type;
    typedef typename traits::socket_type socket_type;
    
    typedef endpoint endpoint_type;
    
    typedef typename endpoint_type::handler_ptr handler_ptr;
    typedef typename endpoint_type::handler handler;
    
    // friends (would require C++11) this would enable connection::start to be 
    // protected instead of public.
    //friend typename endpoint_traits<endpoint_type>::role_type;
    //friend typename endpoint_traits<endpoint_type>::socket_type;
    //friend class role<endpoint>;
    //friend class socket<endpoint>;
    
    friend class role< connection<endpoint,role,socket> >;
    friend class socket< connection<endpoint,role,socket> >;
    
    enum write_state {
        IDLE = 0,
        WRITING = 1,
        INTURRUPT = 2
    };
    
    enum read_state {
        READING = 0,
        WAITING = 1
    };
    
    connection(endpoint_type& e,handler_ptr h) 
     : role_type(e)
     , socket_type(e)
     , m_endpoint(e)
     , m_alog(e.alog_ptr())
     , m_elog(e.elog_ptr())
     , m_handler(h)
     , m_read_threshold(e.get_read_threshold())
     , m_silent_close(e.get_silent_close())
     , m_timer(e.endpoint_base::m_io_service,boost::posix_time::seconds(0))
     , m_state(session::state::CONNECTING)
     , m_protocol_error(false)
     , m_write_buffer(0)
     , m_write_state(IDLE)
     , m_fail_code(fail::status::GOOD)
     , m_local_close_code(close::status::ABNORMAL_CLOSE)
     , m_remote_close_code(close::status::ABNORMAL_CLOSE)
     , m_closed_by_me(false)
     , m_failed_by_me(false)
     , m_dropped_by_me(false)
     , m_read_state(READING)
     , m_strand(e.endpoint_base::m_io_service)
     , m_detached(false)
    {
        socket_type::init();
        
        // This should go away
        m_control_message = message::control_ptr(new message::control());
    }
    
    /// Destroy the connection
    ~connection() {
        try {
            if (m_state != session::state::CLOSED) {
                terminate(true);
            }
        } catch (...) {}
    }
    
    // copy/assignment constructors require C++11
    // boost::noncopyable is being used in the meantime.
    // connection(connection const&) = delete;
    // connection& operator=(connection const&) = delete
    
    /// Start the websocket connection async read loop
    /**
     * Begins the connection's async read loop. First any socket level 
     * initialization will happen (TLS handshake, etc) then the handshake and
     * frame reads will start. 
     * 
     * Visibility: protected
     * State: Should only be called once by the endpoint.
     * Concurrency: safe as long as state is valid
     */
    void start() {
        // initialize the socket.
        socket_type::async_init(
            m_strand.wrap(boost::bind(
                &type::handle_socket_init,
                type::shared_from_this(),
                boost::asio::placeholders::error
            ))
        );
    }
    
    /// Return current connection state
    /**
     * Visibility: public
     * State: valid always
     * Concurrency: callable from anywhere
     *
     * @return Current connection state
     */
    session::state::value get_state() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        return m_state;
    }
    
    /// Detaches the connection from its endpoint
    /**
     * Called by the m_endpoint's destructor. In state DETACHED m_endpoint is
     * no longer avaliable. The connection may stick around if the end user 
     * application needs to read state from it (ie close reasons, etc) but no
     * operations requring the endpoint can be performed.
     * 
     * Visibility: protected
     * State: Should only be called once by the endpoint.
     * Concurrency: safe as long as state is valid
     */
    void detach() {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        m_detached = true;
    }
    
    void send(const std::string& payload, frame::opcode::value op = frame::opcode::TEXT);
    void send(message::data_ptr msg);
    
    /// Close connection
    /**
     * Closes the websocket connection with the given status code and reason.
     * From state OPEN a clean connection close is initiated. From any other
     * state the socket will be closed and the connection cleaned up.
     * 
     * There is no feedback directly from close. Feedback will be provided via
     * the on_fail or on_close callbacks.
     * 
     * Visibility: public
     * State: Valid from OPEN, ignored otherwise
     * Concurrency: callable from any thread
     *
     * @param code Close code to send
     * @param reason Close reason to send
     */
    void close(close::status::value code, const std::string& reason = "") {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        if (m_detached) {return;}
        
        if (m_state == session::state::CONNECTING) {
            m_endpoint.endpoint_base::m_io_service.post(
                m_strand.wrap(boost::bind(
                    &type::terminate,
                    type::shared_from_this(),
                    true
                ))
            );
        } else if (m_state == session::state::OPEN) {
            m_endpoint.endpoint_base::m_io_service.post(
                m_strand.wrap(boost::bind(
                    &type::begin_close,
                    type::shared_from_this(),
                    code,
                    reason
                ))
            );
        } else {
            // in CLOSING state we are already closing, nothing to do
            // in CLOSED state we are already closed, nothing to do
        }
    }
    
    /// Send Ping
    /**
     * Initiates a ping with the given payload.
     * 
     * There is no feedback directly from ping. Feedback will be provided via
     * the on_pong or on_pong_timeout callbacks.
     * 
     * Visibility: public
     * State: Valid from OPEN, ignored otherwise
     * Concurrency: callable from any thread
     *
     * @param payload Payload to be used for the ping
     */
    void ping(const std::string& payload) {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        if (m_state != session::state::OPEN) {return;}
        if (m_detached) {return;}
        
        // TODO: optimize control messages and handle case where 
        // endpoint is out of messages
        message::data_ptr control = get_control_message2();
        control->reset(frame::opcode::PING);
        control->set_payload(payload);
        m_processor->prepare_frame(control);
        
        m_endpoint.endpoint_base::m_io_service.post(
            m_strand.wrap(boost::bind(
                &type::write_message,
                type::shared_from_this(),
                control
            ))
        );
    }
    
    /// Send Pong
    /**
     * Initiates a pong with the given payload.
     * 
     * There is no feedback from pong.
     * 
     * Visibility: public
     * State: Valid from OPEN, ignored otherwise
     * Concurrency: callable from any thread
     *
     * @param payload Payload to be used for the pong
     */
    void pong(const std::string& payload) {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        if (m_state != session::state::OPEN) {return;}
        if (m_detached) {return;}
        
        // TODO: optimize control messages and handle case where 
        // endpoint is out of messages
        message::data_ptr control = get_control_message2();
        control->reset(frame::opcode::PONG);
        control->set_payload(payload);
        m_processor->prepare_frame(control);
        
        m_endpoint.endpoint_base::m_io_service.post(
            m_strand.wrap(boost::bind(
                &type::write_message,
                type::shared_from_this(),
                control
            ))
        );
    }
    
    /// Return send buffer size (payload bytes)
    /**
     * Visibility: public
     * State: Valid from any state.
     * Concurrency: callable from any thread
     * 
     * @return The current number of bytes in the outgoing send buffer.
     */
    uint64_t buffered_amount() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        return m_write_buffer;
    }
    
    /// Get library fail code
    /**
     * Returns the internal WS++ fail code. This code starts at a value of
     * websocketpp::fail::status::GOOD and will be set to other values as errors
     * occur. Some values are direct errors, others point to locations where
     * more specific error information can be found. Key values:
     * 
     * (all in websocketpp::fail::status:: ) namespace
     * GOOD - No error has occurred yet
     * SYSTEM - A system call failed, the system error code is avaliable via
     *          get_fail_system_code()
     * WEBSOCKET - The WebSocket connection close handshake was performed, more 
     *             information is avaliable via get_local_close_code()/reason()
     * UNKNOWN - terminate was called without a more specific error being set
     * 
     * Refer to common.hpp for the rest of the individual codes. Call
     * get_fail_reason() for a human readable error.
     * 
     * Visibility: public
     * State: Valid from any
     * Concurrency: callable from any thread
     *
     * @return Fail code supplied by WebSocket++
     */
    fail::status::value get_fail_code() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        return m_fail_code;
    }
    
    /// Get library failure reason
    /**
     * Returns a human readable library failure reason.
     * 
     * Visibility: public
     * State: Valid from any
     * Concurrency: callable from any thread
     *
     * @return Fail reason supplied by WebSocket++
     */
    std::string get_fail_reason() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        return m_fail_reason;
    }
    
    /// Get system failure code
    /**
     * Returns the system error code that caused WS++ to fail the connection
     * 
     * Visibility: public
     * State: Valid from any
     * Concurrency: callable from any thread
     *
     * @return Error code supplied by system
     */
    boost::system::error_code get_system_fail_code() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        return m_fail_system;
    }
    
    /// Get WebSocket close code sent by WebSocket++
    /**
     * Returns the system error code that caused 
     * 
     * Visibility: public
     * State: Valid from CLOSED, an exception is thrown otherwise
     * Concurrency: callable from any thread
     *
     * @return Close code supplied by WebSocket++
     */
    close::status::value get_local_close_code() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        if (m_state != session::state::CLOSED) {
            throw exception("get_local_close_code called from state other than CLOSED",
                            error::INVALID_STATE);
        }
        
        return m_local_close_code;
    }
    
    /// Get Local Close Reason
    /**
     * Returns the close reason that WebSocket++ believes to be the reason the 
     * connection closed. This is almost certainly the value passed to the 
     * `close()` method.
     * 
     * Visibility: public
     * State: Valid from CLOSED, an exception is thrown otherwise
     * Concurrency: callable from any thread
     *
     * @return Close reason supplied by WebSocket++
     */
    std::string get_local_close_reason() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        if (m_state != session::state::CLOSED) {
            throw exception("get_local_close_reason called from state other than CLOSED",
                            error::INVALID_STATE);
        }
        
        return m_local_close_reason;
    }
    
    /// Get Remote Close Code
    /**
     * Returns the close reason that was received over the wire from the remote peer.
     * This method may return values that are invalid on the wire such as 1005/No close
     * code received, 1006 abnormal closure, or 1015 Bad TLS handshake.
     * 
     * Visibility: public
     * State: Valid from CLOSED, an exception is thrown otherwise
     * Concurrency: callable from any thread
     *
     * @return Close code supplied by remote endpoint
     */
    close::status::value get_remote_close_code() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        if (m_state != session::state::CLOSED) {
            throw exception("get_remote_close_code called from state other than CLOSED",
                            error::INVALID_STATE);
        }
        
        return m_remote_close_code;
    }
    
    /// Get Remote Close Reason
    /**
     * Returns the close reason that was received over the wire from the remote peer.
     * 
     * Visibility: public
     * State: Valid from CLOSED, an exception is thrown otherwise
     * Concurrency: callable from any thread
     * 
     * @return Close reason supplied by remote endpoint
     */
    std::string get_remote_close_reason() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        if (m_state != session::state::CLOSED) {
            throw exception("get_remote_close_reason called from state other than CLOSED",
                            error::INVALID_STATE);
        }
        
        return m_remote_close_reason;
    }
    
    /// Get failed_by_me
    /**
     * Returns whether or not the connection ending sequence was initiated by this 
     * endpoint. Will return true when this endpoint chooses to close normally or when it
     * discovers an error and chooses to close the connection (either forcibly or not).
     * Will return false when the close handshake was initiated by the remote endpoint or
     * if the TCP connection was dropped or broken prematurely.
     * 
     * Visibility: public
     * State: Valid from CLOSED, an exception is thrown otherwise
     * Concurrency: callable from any thread
     *
     * @return Whether or not the connection ending sequence was initiated by this endpoint
     */
    bool get_failed_by_me() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        if (m_state != session::state::CLOSED) {
            throw exception("get_failed_by_me called from state other than CLOSED",
                            error::INVALID_STATE);
        }
        
        return m_failed_by_me;
    }
    
    /// Get dropped_by_me
    /**
     * Returns whether or not the TCP connection was dropped by this endpoint.
     * 
     * Visibility: public
     * State: Valid from CLOSED, an exception is thrown otherwise
     * Concurrency: callable from any thread
     *
     * @return whether or not the TCP connection was dropped by this endpoint.
     */
    bool get_dropped_by_me() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        if (m_state != session::state::CLOSED) {
            throw exception("get_dropped_by_me called from state other than CLOSED",
                            error::INVALID_STATE);
        }
        
        return m_dropped_by_me;
    }
    
    /// Get closed_by_me
    /**
     * Returns whether or not the WebSocket closing handshake was initiated by this 
     * endpoint.
     * 
     * Visibility: public
     * State: Valid from CLOSED, an exception is thrown otherwise
     * Concurrency: callable from any thread
     * 
     * @return Whether or not closing handshake was initiated by this endpoint
     */
    bool get_closed_by_me() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        if (m_state != session::state::CLOSED) {
            throw exception("get_closed_by_me called from state other than CLOSED",
                            error::INVALID_STATE);
        }
        
        return m_closed_by_me;
    }
    
    /// Get get_data_message
    /**
     * Returns a pointer to an outgoing message buffer. If there are no outgoing messages
     * available, a NO_OUTGOING_MESSAGES exception is thrown. This happens when the 
     * endpoint has exhausted its resources dedicated to buffering outgoing messages. To 
     * deal with this error either increase available outgoing resources or throttle back 
     * the rate and size of outgoing messages.
     * 
     * Visibility: public
     * State: Valid from OPEN, an exception is thrown otherwise
     * Concurrency: callable from any thread
     *
     * @return A pointer to the message buffer
     */
    message::data_ptr get_data_message() {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        if (m_detached) {
            throw exception("get_data_message: Endpoint was destroyed",error::ENDPOINT_UNAVAILABLE);
        }
        
        if (m_state != session::state::OPEN && m_state != session::state::CLOSING) {
            throw exception("get_data_message called from invalid state",
                            error::INVALID_STATE);
        }
        
        message::data_ptr msg = m_endpoint.get_data_message();
        
        if (!msg) {
            throw exception("No outgoing messages available",error::NO_OUTGOING_MESSAGES);
        } else {
            return msg;
        }
    }
    
    
    message::data_ptr get_control_message2() {
        return m_endpoint.get_control_message();
    }
    
    message::control_ptr get_control_message() {
        return m_control_message;
    }
    
    /// Set connection handler
    /**
     * Sets the handler that will process callbacks for this connection. The switch is
     * processed asynchronously so set_handler will return immediately. The existing 
     * handler will receive an on_unload callback immediately before the switch. After 
     * on_unload returns the original handler will not receive any more callbacks from 
     * this connection. The new handler will receive an on_load callback immediately after
     * the switch and before any other callbacks are processed.
     * 
     * Visibility: public
     * State: Valid from any state
     * Concurrency: callable from any thread
     *
     * @param new_handler the new handler to set
     */
    void set_handler(handler_ptr new_handler) {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        if (m_detached) {return;}
        
        if (!new_handler) {
            elog()->at(log::elevel::FATAL) << "Tried to switch to a NULL handler." 
                                           << log::endl;
            terminate(true);
            return;
        }
        
        m_endpoint.endpoint_base::m_io_service.post(
            m_strand.wrap(boost::bind(
                &type::set_handler_internal,
                type::shared_from_this(),
                new_handler
            ))
        );
    }
    
    /// Set connection read threshold
    /**
     * Set the read threshold for this connection. See endpoint::set_read_threshold for 
     * more information about the read threshold.
     * 
     * Visibility: public
     * State: valid always
     * Concurrency: callable from anywhere
     * 
     * @param val Size of the threshold in bytes
     */
    void set_read_threshold(size_t val) {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        m_read_threshold = val;
    }
    
    /// Get connection read threshold
    /**
     * Returns the connection read threshold. See set_read_threshold for more information
     * about the read threshold.
     * 
     * Visibility: public
     * State: valid always
     * Concurrency: callable from anywhere
     * 
     * @return Size of the threshold in bytes
     */
    size_t get_read_threshold() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        return m_read_threshold;
    }
    
    /// Set connection silent close setting
    /**
     * See endpoint::set_silent_close for more information.
     * 
     * Visibility: public
     * State: valid always
     * Concurrency: callable from anywhere
     * 
     * @param val New silent close value
     */
    void set_silent_close(bool val) {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        m_silent_close = val;
    }
    
    /// Get connection silent close setting
    /**
     * Visibility: public
     * State: valid always
     * Concurrency: callable from anywhere
     * 
     * @return Current silent close value
     * @see set_silent_close()
     */
    bool get_silent_close() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        return m_silent_close;
    }
    
    // TODO: deprecated. will change to get_rng?
    int32_t gen() {
        return 0;
    }
    
    /// Returns a reference to the endpoint's access logger.
    /**
     * Visibility: public
     * State: Valid from any state
     * Concurrency: callable from any thread
     *
     * @return A reference to the endpoint's access logger
     */
    typename endpoint::alogger_ptr alog() {
        return m_alog;
    }
    
    /// Returns a reference to the endpoint's error logger.
    /**
     * Visibility: public
     * State: Valid from any state
     * Concurrency: callable from any thread
     *
     * @return A reference to the endpoint's error logger
     */
    typename endpoint::elogger_ptr elog() {
        return m_elog;
    }
    
    /// Returns a pointer to the endpoint's handler.
    /**
     * Visibility: public
     * State: Valid from any state
     * Concurrency: callable from any thread
     *
     * @return A pointer to the endpoint's default handler
     */
    handler_ptr get_handler() {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        return m_handler;
    }
    
    /// Returns a reference to the connections strand object.
    /**
     * Visibility: public
     * State: Valid from any state
     * Concurrency: callable from any thread
     *
     * @return A reference to the connection's strand object
     */
    boost::asio::strand& get_strand() {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        return m_strand;
    }
public:
//protected:  TODO: figure out why VCPP2010 doesn't like protected here

    /// Socket initialization callback
    /* This is the async return point for initializing the socket policy. After this point
     * the socket is open and ready.
     * 
     * Visibility: protected
     * State: Valid only once per connection during the initialization sequence.
     * Concurrency: Must be called within m_strand
     */
    void handle_socket_init(const boost::system::error_code& error) {
        if (error) {
            elog()->at(log::elevel::RERROR) 
                << "Socket initialization failed, error code: " << error 
                << log::endl;
            this->terminate(false);
            return;
        }
        
        role_type::async_init();
    }
public: 
    /// ASIO callback for async_read of more frame data
    /**
     * Callback after ASIO has read some data that needs to be sent to a frame processor
     * 
     * TODO: think about how receiving a very large buffer would affect concurrency due to
     *       that handler running for an unusually long period of time? Is a maximum size
     *       necessary on m_buf?
     * TODO: think about how terminate here works with the locks and concurrency
     * 
     * Visibility: protected
     * State: valid for states OPEN and CLOSING, ignored otherwise
     * Concurrency: must be called via strand. Only one async_read should be outstanding
     *              at a time. Should only be called from inside handle_read_frame or by
     *              the role's init method.
     */
    void handle_read_frame(const boost::system::error_code& error) {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        // check if state changed while we were waiting for a read.
        if (m_state == session::state::CLOSED) {
            alog()->at(log::alevel::DEVEL) 
                << "handle read returning due to closed connection" 
                << log::endl;
            return;
        }
        if (m_state == session::state::CONNECTING) { return; }
        
        if (error) {
            if (error == boost::asio::error::eof) {         
                elog()->at(log::elevel::RERROR) 
                    << "Unexpected EOF from remote endpoint, terminating connection." 
                    << log::endl;
                terminate(false);
                return;
            } else if (error == boost::asio::error::operation_aborted) {
                // got unexpected abort (likely our server issued an abort on
                // all connections on this io_service)
                
                elog()->at(log::elevel::RERROR) 
                    << "Connection terminating due to aborted read: " 
                    << error << log::endl;
                terminate(true);
                return;
            } else {
                // Other unexpected error
                
                elog()->at(log::elevel::RERROR) 
                    << "Connection terminating due to unknown error: " << error 
                    << log::endl;
                terminate(false);
                return;
            }
        }
        
        // process data from the buffer just read into
        std::istream s(&m_buf);
                
        while (m_state != session::state::CLOSED && m_buf.size() > 0) {
            try {
                m_processor->consume(s);
                
                if (m_processor->ready()) {
                    if (m_processor->is_control()) {
                        process_control(m_processor->get_control_message());
                    } else {
                        process_data(m_processor->get_data_message());
                    }                   
                    m_processor->reset();
                }
            } catch (const processor::exception& e) {
                if (m_processor->ready()) {
                    m_processor->reset();
                }
                
                switch(e.code()) {
                    // the protocol error flag is set by any processor exception
                    // that indicates that the composition of future bytes in 
                    // the read stream cannot be reliably determined. Bytes will
                    // no longer be read after that point.
                    case processor::error::PROTOCOL_VIOLATION:
                        m_protocol_error = true;
                        begin_close(close::status::PROTOCOL_ERROR,e.what());
                        break;
                    case processor::error::PAYLOAD_VIOLATION:
                        m_protocol_error = true;
                        begin_close(close::status::INVALID_PAYLOAD,e.what());
                        break;
                    case processor::error::INTERNAL_ENDPOINT_ERROR:
                        m_protocol_error = true;
                        begin_close(close::status::INTERNAL_ENDPOINT_ERROR,e.what());
                        break;
                    case processor::error::SOFT_ERROR:
                        continue;
                    case processor::error::MESSAGE_TOO_BIG:
                        m_protocol_error = true;
                        begin_close(close::status::MESSAGE_TOO_BIG,e.what());
                        break;
                    case processor::error::OUT_OF_MESSAGES:
                        // we need to wait for a message to be returned by the
                        // client. We exit the read loop. handle_read_frame
                        // will be restarted by recycle()
                        //m_read_state = WAITING;
                        m_endpoint.wait(type::shared_from_this());
                        return;
                    default:
                        // Fatal error, forcibly end connection immediately.
                        elog()->at(log::elevel::DEVEL) 
                            << "Terminating connection due to unrecoverable processor exception: " 
                            << e.code() << " (" << e.what() << ")" << log::endl;
                        terminate(true);
                }
                break;
                
            }
        }
        
        // try and read more
        if (m_state != session::state::CLOSED && 
            m_processor->get_bytes_needed() > 0 && 
            !m_protocol_error)
        {
            // TODO: read timeout timer?
            
            boost::asio::async_read(
                socket_type::get_socket(),
                m_buf,
                boost::asio::transfer_at_least(std::min(
                    m_read_threshold,
                    static_cast<size_t>(m_processor->get_bytes_needed())
                )),
                m_strand.wrap(boost::bind(
                    &type::handle_read_frame,
                    type::shared_from_this(),
                    boost::asio::placeholders::error
                ))
            );
        }
    }
public:
//protected:  TODO: figure out why VCPP2010 doesn't like protected here 
    /// Internal Implementation for set_handler
    /**
     * Visibility: protected
     * State: Valid for all states
     * Concurrency: Must be called within m_strand
     *
     * @param new_handler The handler to switch to
     */
    void set_handler_internal(handler_ptr new_handler) {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        if (!new_handler) {            
            elog()->at(log::elevel::FATAL) 
                << "Terminating connection due to attempted switch to NULL handler." 
                << log::endl;
            // TODO: unserialized call to terminate?
            terminate(true);
            return;
        }
        
        handler_ptr old_handler = m_handler;
        
        old_handler->on_unload(type::shared_from_this(),new_handler);
        m_handler = new_handler;
        new_handler->on_load(type::shared_from_this(),old_handler);
    }
    
    /// Dispatch a data message
    /** 
     * Visibility: private
     * State: no state checking, should only be called within handle_read_frame
     * Concurrency: Must be called within m_stranded method
     */
    void process_data(message::data_ptr msg) {
        m_handler->on_message(type::shared_from_this(),msg);
    }
    
    /// Dispatch or handle a control message
    /** 
     * Visibility: private
     * State: no state checking, should only be called within handle_read_frame
     * Concurrency: Must be called within m_stranded method
     */
    void process_control(message::control_ptr msg) {
        bool response;
        switch (msg->get_opcode()) {
            case frame::opcode::PING:
                response = m_handler->on_ping(type::shared_from_this(),
                                              msg->get_payload());
                if (response) {
                    pong(msg->get_payload());
                }
                break;
            case frame::opcode::PONG:
                m_handler->on_pong(type::shared_from_this(),
                                   msg->get_payload());
                // TODO: disable ping response timer
                
                break;
            case frame::opcode::CLOSE:
                m_remote_close_code = msg->get_close_code();
                m_remote_close_reason = msg->get_close_reason();
                
                // check that the codes we got over the wire are valid
                
                if (m_state == session::state::OPEN) {
                    // other end is initiating
                    alog()->at(log::alevel::DEBUG_CLOSE) 
                        << "sending close ack" << log::endl;
                    
                    // TODO:
                    send_close_ack();
                } else if (m_state == session::state::CLOSING) {
                    // ack of our close
                    alog()->at(log::alevel::DEBUG_CLOSE) 
                        << "got close ack" << log::endl;
                    
                    terminate(false);
                    // TODO: start terminate timer (if client)
                }
                break;
            default:
                throw processor::exception("Invalid Opcode",
                    processor::error::PROTOCOL_VIOLATION);
                break;
        }
    }
    
    /// Begins a clean close handshake
    /**
     * Initiates a close handshake by sending a close frame with the given code 
     * and reason. 
     * 
     * Visibility: protected
     * State: Valid for OPEN, ignored otherwise.
     * Concurrency: Must be called within m_strand
     *
     * @param code The code to send
     * @param reason The reason to send
     */
    void begin_close(close::status::value code, const std::string& reason) {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        alog()->at(log::alevel::DEBUG_CLOSE) << "begin_close called" << log::endl;
        
        if (m_detached) {return;}
        
        if (m_state != session::state::OPEN) {
            elog()->at(log::elevel::WARN) 
                << "Tried to disconnect a session that wasn't open" << log::endl;
            return;
        }
        
        if (close::status::invalid(code)) {
            elog()->at(log::elevel::WARN) 
                << "Tried to close a connection with invalid close code: " 
                << code << log::endl;
            return;
        } else if (close::status::reserved(code)) {
            elog()->at(log::elevel::WARN) 
                << "Tried to close a connection with reserved close code: " 
                << code << log::endl;
            return;
        }
        
        m_state = session::state::CLOSING;
        
        m_closed_by_me = true;
        
        if (m_silent_close) {
            m_local_close_code = close::status::NO_STATUS;
            m_local_close_reason = "";
            
            // In case of protocol error in silent mode just drop the connection.
            // This is allowed by the spec and improves robustness over sending an
            // empty close frame that we would ignore the ack to.
            if (m_protocol_error) {
                terminate(false);
                return;
            }
        } else {
            m_local_close_code = code;
            m_local_close_reason = reason;
        }
        
        // wait for close timer
        // TODO: configurable value
        register_timeout(5000,fail::status::WEBSOCKET,"Timeout on close handshake");
        
        // TODO: optimize control messages and handle case where endpoint is
        // out of messages
        message::data_ptr msg = get_control_message2();
        
        if (!msg) {
            // Server endpoint is out of control messages. Force drop connection
            elog()->at(log::elevel::RERROR) 
                << "Request for control message failed (out of resources). Terminating connection."
                << log::endl;
            terminate(true);
            return;
        }
        
        msg->reset(frame::opcode::CLOSE);
        m_processor->prepare_close_frame(msg,m_local_close_code,m_local_close_reason);
        
        m_endpoint.endpoint_base::m_io_service.post(
            m_strand.wrap(boost::bind(
                &type::write_message,
                type::shared_from_this(),
                msg
            ))
        );
    }
    
    /// send an acknowledgement close frame
    /** 
     * Visibility: private
     * State: no state checking, should only be called within process_control
     * Concurrency: Must be called within m_stranded method
     */
    void send_close_ack() {
        alog()->at(log::alevel::DEBUG_CLOSE) << "send_close_ack called" << log::endl;
        
        // echo close value unless there is a good reason not to.
        if (m_silent_close) {
            m_local_close_code = close::status::NO_STATUS;
            m_local_close_reason = "";
        } else if (m_remote_close_code == close::status::NO_STATUS) {
            m_local_close_code = close::status::NORMAL;
            m_local_close_reason = "";
        } else if (m_remote_close_code == close::status::ABNORMAL_CLOSE) {
            // TODO: can we possibly get here? This means send_close_ack was
            //       called after a connection ended without getting a close
            //       frame
            throw "shouldn't be here";
        } else if (close::status::invalid(m_remote_close_code)) {
            // TODO: shouldn't be able to get here now either
            m_local_close_code = close::status::PROTOCOL_ERROR;
            m_local_close_reason = "Status code is invalid";
        } else if (close::status::reserved(m_remote_close_code)) {
            // TODO: shouldn't be able to get here now either
            m_local_close_code = close::status::PROTOCOL_ERROR;
            m_local_close_reason = "Status code is reserved";
        } else {
            m_local_close_code = m_remote_close_code;
            m_local_close_reason = m_remote_close_reason;
        }
        
        // TODO: check whether we should cancel the current in flight write.
        //       if not canceled the close message will be sent as soon as the
        //       current write completes.
        
        
        // TODO: optimize control messages and handle case where endpoint is
        // out of messages
        message::data_ptr msg = get_control_message2();
        
        if (!msg) {
            // server is out of resources, close connection.
            elog()->at(log::elevel::RERROR) 
                << "Request for control message failed (out of resources). Terminating connection."
                << log::endl;
            terminate(true);
            return;
        }
        
        msg->reset(frame::opcode::CLOSE);
        m_processor->prepare_close_frame(msg,m_local_close_code,m_local_close_reason);
        
        m_endpoint.endpoint_base::m_io_service.post(
            m_strand.wrap(boost::bind(
                &type::write_message,
                type::shared_from_this(),
                msg
            ))
        );
        //m_write_state = INTURRUPT;        
    }
    
    /// Push message to write queue and start writer if it was idle
    /** 
     * Visibility: protected (called only by asio dispatcher)
     * State: Valid from OPEN and CLOSING, ignored otherwise
     * Concurrency: Must be called within m_stranded method
     */
    void write_message(message::data_ptr msg) {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        if (m_state != session::state::OPEN && m_state != session::state::CLOSING) {return;}
        if (m_write_state == INTURRUPT) {return;}
        
        m_write_buffer += msg->get_payload().size();
        m_write_queue.push(msg);
        
        write();
    }
    
    /// Begin async write of next message in list
    /** 
     * Visibility: private
     * State: Valid only as called from write_message or handle_write
     * Concurrency: Must be called within m_stranded method
     */
    void write() {
        switch (m_write_state) {
            case IDLE:
                break;
            case WRITING:
                // already writing. write() will get called again by the write
                // handler once it is ready.
                return;
            case INTURRUPT:
                // clear the queue except for the last message
                while (m_write_queue.size() > 1) {
                    m_write_buffer -= m_write_queue.front()->get_payload().size();
                    m_write_queue.pop();
                }
                break;
            default:
                // TODO: assert shouldn't be here
                break;
        }
        
        if (!m_write_queue.empty()) {
            if (m_write_state == IDLE) {
                m_write_state = WRITING;
            }
                        
            m_write_buf.push_back(boost::asio::buffer(m_write_queue.front()->get_header()));
            m_write_buf.push_back(boost::asio::buffer(m_write_queue.front()->get_payload()));
            
            //m_endpoint.alog().at(log::alevel::DEVEL) << "write header: " << zsutil::to_hex(m_write_queue.front()->get_header()) << log::endl;
            
            boost::asio::async_write(
                socket_type::get_socket(),
                m_write_buf,
                m_strand.wrap(boost::bind(
                    &type::handle_write,
                    type::shared_from_this(),
                    boost::asio::placeholders::error
                ))
            );
        } else {
            // if we are in an inturrupted state and had nothing else to write
            // it is safe to terminate the connection.
            if (m_write_state == INTURRUPT) {
                alog()->at(log::alevel::DEBUG_CLOSE) 
                    << "Exit after inturrupt" << log::endl;
                terminate(false);
            }
        }
    }
    
    /// async write callback
    /** 
     * Visibility: protected (called only by asio dispatcher)
     * State: Valid from OPEN and CLOSING, ignored otherwise
     * Concurrency: Must be called within m_stranded method
     */
    void handle_write(const boost::system::error_code& error) {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        
        
        if (error) {
            if (m_state == session::state::CLOSED) {
                alog()->at(log::alevel::DEBUG_CLOSE) 
                        << "handle_write error in CLOSED state. Ignoring." 
                        << log::endl;
                return;
            }
            
            if (error == boost::asio::error::operation_aborted) {
                // previous write was aborted
                alog()->at(log::alevel::DEBUG_CLOSE) 
                    << "ASIO write was aborted. Exiting write loop." 
                    << log::endl;
                //terminate(false);
                return;
            } else {
                log_error("ASIO write failed with unknown error. Terminating connection.",error);
                terminate(false);
                return;
            }
        }
        
        
        
        if (m_write_queue.size() == 0) {
            alog()->at(log::alevel::DEBUG_CLOSE) 
                << "handle_write called with empty queue" << log::endl;
            return;
        }
        
        m_write_buffer -= m_write_queue.front()->get_payload().size();
        m_write_buf.clear();
        
        frame::opcode::value code = m_write_queue.front()->get_opcode();
        
        m_write_queue.pop();
        
        if (m_write_state == WRITING) {
            m_write_state = IDLE;
        }
        
        
        
        
        if (code != frame::opcode::CLOSE) {
            // only continue next write if the connection is still open
            if (m_state == session::state::OPEN || m_state == session::state::CLOSING) {
                write();
            }
        } else {
            if (m_closed_by_me) {
                alog()->at(log::alevel::DEBUG_CLOSE) 
                        << "Initial close frame sent" << log::endl;
                // this is my close message
                // no more writes allowed after this. will hang on to read their
                // close response unless I just sent a protocol error close, in
                // which case I assume the other end is too broken to return
                // a meaningful response.
                if (m_protocol_error) {
                    terminate(false);
                }
            } else {
                // this is a close ack
                // now that it has been written close the connection
                
                if (m_endpoint.is_server()) {
                    // if I am a server close immediately
                    alog()->at(log::alevel::DEBUG_CLOSE) 
                        << "Close ack sent. Terminating immediately." << log::endl;
                    terminate(false);
                } else {
                    // if I am a client give the server a moment to close.
                    alog()->at(log::alevel::DEBUG_CLOSE) 
                        << "Close ack sent. Termination queued." << log::endl;
                    // TODO: set timer here and close afterwards
                    terminate(false);
                }
            }
        }
    }
    
    /// Ends the connection by cleaning up based on current state
    /** Terminate will review the outstanding resources and close each
     * appropriately. Attached handlers will recieve an on_fail or on_close call
     * 
     * TODO: should we protect against long running handlers?
     * 
     * Visibility: protected
     * State: Valid from any state except CLOSED.
     * Concurrency: Must be called from within m_strand
     *
     * @param failed_by_me Whether or not to mark the connection as failed by me
     */
    void terminate(bool failed_by_me) {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        alog()->at(log::alevel::DEVEL) 
                << "terminate called from state: " << m_state << log::endl;
        
        // If state is closed it either means terminate was called twice or
        // something other than this library called it. In either case running
        // it will only cause problems
        if (m_state == session::state::CLOSED) {return;}
        
        // TODO: ensure any other timers are accounted for here.
        // cancel the close timeout
        cancel_timeout();
        
        // version -1 is an HTTP (non-websocket) connection.
        if (role_type::get_version() != -1) {
            // TODO: note, calling shutdown on the ssl socket for an HTTP
            // connection seems to cause shutdown to block for a very long time.
            // NOT calling it for a websocket connection causes the connection
            // to time out. Behavior now is correct but I am not sure why.
            m_dropped_by_me = socket_type::shutdown();
            
            m_failed_by_me = failed_by_me;
            
            session::state::value old_state = m_state;
            m_state = session::state::CLOSED;
            
            if (old_state == session::state::CONNECTING) {
                m_handler->on_fail(type::shared_from_this());
                
                if (m_fail_code == fail::status::GOOD) {
                    m_fail_code = fail::status::UNKNOWN;
                    m_fail_reason = "Terminate called in connecting state without more specific error.";
                }
            } else if (old_state == session::state::OPEN || 
                       old_state == session::state::CLOSING)
            {
                m_handler->on_close(type::shared_from_this());
                
                if (m_fail_code == fail::status::GOOD) {
                    m_fail_code = fail::status::WEBSOCKET;
                    m_fail_reason = "Terminate called in open state without more specific error.";
                }
            }
            
            log_close_result(); 
        }
        
        // finally remove this connection from the endpoint's list. This will
        // remove the last shared pointer to the connection held by WS++. If we
        // are DETACHED this has already been done and can't be done again.
        if (!m_detached) {
            alog()->at(log::alevel::DEVEL) 
                << "terminate removing connection" << log::endl;
            m_endpoint.remove_connection(type::shared_from_this());
            
            /*m_endpoint.endpoint_base::m_io_service.post(
                m_strand.wrap(boost::bind(
                    &type::remove_connection,
                    type::shared_from_this()
                ))
            );*/
        }
    }
    
    // This was an experiment in deferring the detachment of the connection 
    // until all handlers had been cleaned up. With the switch to a local logger
    // this can probably be removed entirely.
    void remove_connection() {
        // finally remove this connection from the endpoint's list. This will
        // remove the last shared pointer to the connection held by WS++. If we
        // are DETACHED this has already been done and can't be done again.
        /*if (!m_detached) {
            alog()->at(log::alevel::DEVEL) << "terminate removing connection: start" << log::endl;
            m_endpoint.remove_connection(type::shared_from_this());
        }*/
    }
    
    // this is called when an async asio call encounters an error
    void log_error(std::string msg,const boost::system::error_code& e) {
        elog()->at(log::elevel::RERROR) << msg << "(" << e << ")" << log::endl;
    }
    
    void log_close_result() {
        alog()->at(log::alevel::DISCONNECT) 
            //<< "Disconnect " << (m_was_clean ? "Clean" : "Unclean")
            << "Disconnect " 
            << " close local:[" << m_local_close_code
            << (m_local_close_reason == "" ? "" : ","+m_local_close_reason) 
            << "] remote:[" << m_remote_close_code 
            << (m_remote_close_reason == "" ? "" : ","+m_remote_close_reason) << "]"
            << log::endl;
     }
    
    void register_timeout(size_t ms,fail::status::value s, std::string msg) {
        m_timer.expires_from_now(boost::posix_time::milliseconds(ms));
        m_timer.async_wait(
            m_strand.wrap(boost::bind(
                &type::fail_on_expire,
                type::shared_from_this(),
                boost::asio::placeholders::error,
                s,
                msg
            ))
        );
    }
    
    void cancel_timeout() {
         m_timer.cancel();
    }
    
    void fail_on_expire(const boost::system::error_code& error,
                        fail::status::value status,
                        std::string& msg)
    {
        if (error) {
            if (error != boost::asio::error::operation_aborted) {
                elog()->at(log::elevel::DEVEL) 
                    << "fail_on_expire timer ended in unknown error" << log::endl;
                terminate(false);
            }
            return;
        }
        
        m_fail_code = status;
        m_fail_system = error;
        m_fail_reason = msg;
        
        alog()->at(log::alevel::DISCONNECT) 
            << "fail_on_expire timer expired with message: " << msg << log::endl;
        terminate(true);
    }
    
    boost::asio::streambuf& buffer()  {
        return m_buf;
    }
public:
//protected:  TODO: figure out why VCPP2010 doesn't like protected here
    endpoint_type&                  m_endpoint;
    typename endpoint::alogger_ptr  m_alog;
    typename endpoint::elogger_ptr  m_elog;
    
    // Overridable connection specific settings
    handler_ptr                 m_handler;          // object to dispatch callbacks to
    size_t                      m_read_threshold;   // maximum number of bytes to fetch in
                                                    //   a single async read.
    bool                        m_silent_close;     // suppresses the return of detailed 
                                                    //   close codes.
    
    // Network resources
    boost::asio::streambuf      m_buf;
    boost::asio::deadline_timer m_timer;
    
    // WebSocket connection state
    session::state::value       m_state;
    bool                        m_protocol_error;
    
    // stuff that actually does the work
    processor::ptr              m_processor;
    
    // Write queue
    std::vector<boost::asio::const_buffer> m_write_buf;
    std::queue<message::data_ptr>   m_write_queue;
    uint64_t                        m_write_buffer;
    write_state                     m_write_state;
    
    // Close state
    fail::status::value         m_fail_code;
    boost::system::error_code   m_fail_system;
    std::string                 m_fail_reason;
    close::status::value        m_local_close_code;
    std::string                 m_local_close_reason;
    close::status::value        m_remote_close_code;
    std::string                 m_remote_close_reason;
    bool                        m_closed_by_me;
    bool                        m_failed_by_me;
    bool                        m_dropped_by_me;
    
    // Read queue
    read_state                  m_read_state;
    message::control_ptr        m_control_message;
    
    // concurrency support
    mutable boost::recursive_mutex      m_lock;
    boost::asio::strand         m_strand;
    bool                        m_detached; // TODO: this should be atomic
};

// connection related types that it and its policy classes need.
template <
    typename endpoint,
    template <class> class role,
    template <class> class socket>
struct connection_traits< connection<endpoint,role,socket> > {
    // type of the connection itself
    typedef connection<endpoint,role,socket> type;
    typedef boost::shared_ptr<type> ptr;
    
    // types of the connection policies
    typedef endpoint endpoint_type;
    typedef role< type > role_type;
    typedef socket< type > socket_type;
};

/// convenience overload for sending a one off message.
/**
 * Creates a message, fills in payload, and queues a write as a message of
 * type op. Default type is TEXT. 
 * 
 * Visibility: public
 * State: Valid from OPEN, ignored otherwise
 * Concurrency: callable from any thread
 *
 * @param payload Payload to write_state
 * @param op opcode to send the message as
 */
template <typename endpoint,template <class> class role,template <class> class socket>
void 
connection<endpoint,role,socket>::send(const std::string& payload,frame::opcode::value op)
{
    {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        if (m_state != session::state::OPEN) {return;}
    }
    
    websocketpp::message::data::ptr msg = get_control_message2();
    
    if (!msg) {
        throw exception("Endpoint send queue is full",error::SEND_QUEUE_FULL);
    }
    if (op != frame::opcode::TEXT && op != frame::opcode::BINARY) {
        throw exception("opcode must be either TEXT or BINARY",error::GENERIC);
    }
    
    msg->reset(op);
    msg->set_payload(payload);
    send(msg);
}

/// Send message
/**
 * Prepares (if necessary) and sends the given message 
 * 
 * Visibility: public
 * State: Valid from OPEN, ignored otherwise
 * Concurrency: callable from any thread
 *
 * @param msg A pointer to a data message buffer to send.
 */
template <typename endpoint,template <class> class role,template <class> class socket>
void connection<endpoint,role,socket>::send(message::data_ptr msg) {
    boost::lock_guard<boost::recursive_mutex> lock(m_lock);
    if (m_state != session::state::OPEN) {return;}
    
    m_processor->prepare_frame(msg);
    
    m_endpoint.endpoint_base::m_io_service.post(
        m_strand.wrap(boost::bind(
            &type::write_message,
            type::shared_from_this(),
            msg
        ))
    );
}

} // namespace websocketpp

#endif // WEBSOCKETPP_CONNECTION_HPP
