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
     : role_type(e),
       socket_type(e),
       m_endpoint(e),
       m_handler(h),
       m_timer(e.endpoint_base::m_io_service,boost::posix_time::seconds(0)),
       m_state(session::state::CONNECTING),
       m_write_buffer(0),
       m_write_state(IDLE),
       m_remote_close_code(close::status::ABNORMAL_CLOSE),
       m_read_state(READING)
    {
        socket_type::init();
        
        m_control_message = message::control_ptr(new message::control());
    }
    
    // SHOULD BE PROTECTED
    void start() {
        // initialize the socket.
        socket_type::async_init(
            boost::bind(
                &type::handle_socket_init,
                type::shared_from_this(),
                boost::asio::placeholders::error
            )
        );
    }
    // END PROTECTED
    
    // Valid always
    session::state::value get_state() const {
        return m_state;
    }
    
    // Valid for OPEN state
    /// convenience overload for sending a one off text message.
    void send(const std::string& payload, frame::opcode::value op = frame::opcode::TEXT) {
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
    void send(message::data_ptr msg) {
        m_processor->prepare_frame(msg);
        write_message(msg);
    }
    
    // TODO: overloads without code or reason?
    void close(close::status::value code, const std::string& reason = "") {
        if (m_state == session::state::CONNECTING) {
            terminate(true);
        } else {
            send_close(code, reason);
        }
    }
    void ping(const std::string& payload) {
        send_ping(payload);
    }
    void pong(const std::string& payload) {
        send_pong(payload);
    }
    
    uint64_t buffered_amount() const {
        return m_write_buffer;
    }
    
    // Valid for CLOSED state
    close::status::value get_local_close_code() const {
        return m_local_close_code;
    }
    utf8_string get_local_close_reason() const {
        return m_local_close_reason;
    }
    close::status::value get_remote_close_code() const {
        return m_remote_close_code;
    }
    utf8_string get_remote_close_reason() const {
        return m_remote_close_reason;
    }
    bool get_failed_by_me() const {
        return m_failed_by_me;
    }
    bool get_dropped_by_me() const {
        return m_dropped_by_me;
    }
    bool get_closed_by_me() const {
        return m_closed_by_me;
    }
    
    // flow control interface
    message::data_ptr get_data_message() {
        return m_endpoint.get_data_message();
    }
    
    message::data_ptr get_control_message2() {
        return m_endpoint.get_control_message();
    }
    
    message::control_ptr get_control_message() {
        return m_control_message;
    }
    
    
    // stuff about switching handlers on the fly
    // TODO: organize more  
    void set_handler(handler_ptr new_handler) {
        
        if (!new_handler) {
            m_endpoint.elog().at(log::elevel::FATAL) 
                << "Tried to switch to a NULL handler." << log::endl;
            terminate(true);
            return;
        }
        
        handler_ptr old_handler = get_handler();
        
        old_handler->on_unload(type::shared_from_this(),new_handler);
        m_handler = new_handler;
        new_handler->on_load(type::shared_from_this(),old_handler);
    }
    
    // TODO: deprecated. will change to get_rng?
    int32_t gen() {
        return 0;
    }
    
    typename endpoint::alogger_type& alog() {
        return m_endpoint.alog();
    }
protected:  
    void handle_socket_init(const boost::system::error_code& error) {
        if (error) {
            m_endpoint.elog().at(log::elevel::ERROR) 
                << "Socket initialization failed, error code: " << error 
                << log::endl;
            this->terminate(false);
            return;
        }
        
        role_type::async_init();
    }
public: 
    void handle_read_frame(const boost::system::error_code& error) {
        // check if state changed while we were waiting for a read.
        if (m_state == session::state::CLOSED) { return; }
        
        if (error) {
            if (error == boost::asio::error::eof) {         
                // got unexpected EOF
                // TODO: log error
                m_endpoint.elog().at(log::elevel::ERROR) 
                    << "Remote connection dropped unexpectedly" << log::endl;
                terminate(false);
            } else if (error == boost::asio::error::operation_aborted) {
                // got unexpected abort (likely our server issued an abort on
                // all connections on this io_service)
                
                // TODO: log error
                m_endpoint.elog().at(log::elevel::ERROR) 
                    << "Terminating due to abort: " << error << log::endl;
                terminate(true);
            } else {
                // Other unexpected error
                
                // TODO: log error
                m_endpoint.elog().at(log::elevel::ERROR) 
                    << "Terminating due to unknown error: " << error 
                    << log::endl;
                terminate(false);
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
                    case processor::error::PROTOCOL_VIOLATION:
                        send_close(close::status::PROTOCOL_ERROR, e.what());
                        break;
                    case processor::error::PAYLOAD_VIOLATION:
                        send_close(close::status::INVALID_PAYLOAD, e.what());
                        break;
                    case processor::error::INTERNAL_ENDPOINT_ERROR:
                        send_close(close::status::INTERNAL_ENDPOINT_ERROR, e.what());
                        break;
                    case processor::error::SOFT_ERROR:
                        continue;
                    case processor::error::MESSAGE_TOO_BIG:
                        send_close(close::status::MESSAGE_TOO_BIG, e.what());
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
                        m_endpoint.elog().at(log::elevel::DEVEL) 
                        << "Dropping TCP due to unrecoverable exception: "
                        << e.code()
                        << " ("
                        << e.what()
                        << ")" 
                        << log::endl;
                        terminate(true);
                }
                break;
                
            }
        }
        
        // try and read more
        if (m_state != session::state::CLOSED && 
            m_processor->get_bytes_needed() > 0) {
            // TODO: read timeout timer?
            
            boost::asio::async_read(
                socket_type::get_socket(),
                m_buf,
                boost::asio::transfer_at_least(m_processor->get_bytes_needed()),
                boost::bind(
                    &type::handle_read_frame,
                    type::shared_from_this(),
                    boost::asio::placeholders::error
                )
            );
        }
    }
protected:  
    void process_data(message::data_ptr msg) {
        get_handler()->on_message(type::shared_from_this(),msg);
    }
    
    void process_control(message::control_ptr msg) {
        bool response;
        switch (msg->get_opcode()) {
            case frame::opcode::PING:
                response = get_handler()->on_ping(type::shared_from_this(),
                                                  msg->get_payload());
                if (response) {
                    send_pong(msg->get_payload());
                }
                break;
            case frame::opcode::PONG:
                get_handler()->on_pong(type::shared_from_this(),
                                       msg->get_payload());
                // TODO: disable ping response timer
                
                break;
            case frame::opcode::CLOSE:
                m_remote_close_code = msg->get_close_code();
                m_remote_close_reason = msg->get_close_reason();
                
                // check that the codes we got over the wire are valid
                
                if (m_state == session::state::OPEN) {
                    // other end is initiating
                    m_endpoint.elog().at(log::elevel::DEVEL) 
                    << "sending close ack" << log::endl;
                    
                    // TODO:
                    send_close_ack();
                } else if (m_state == session::state::CLOSING) {
                    // ack of our close
                    m_endpoint.elog().at(log::elevel::DEVEL) 
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
    
    void send_close(close::status::value code, const std::string& reason) {
        if (m_state != session::state::OPEN) {
            m_endpoint.elog().at(log::elevel::WARN) 
                << "Tried to disconnect a session that wasn't open" 
                << log::endl;
            return;
        }
        
        if (close::status::invalid(code)) {
            m_endpoint.elog().at(log::elevel::WARN) 
                << "Tried to close a connection with invalid close code: " 
                << code << log::endl;
            return;
        } else if (close::status::reserved(code)) {
            m_endpoint.elog().at(log::elevel::WARN) 
                << "Tried to close a connection with reserved close code: " 
                << code << log::endl;
            return;
        }
        
        m_state = session::state::CLOSING;
        
        m_closed_by_me = true;
        
        m_timer.expires_from_now(boost::posix_time::milliseconds(1000));
        m_timer.async_wait(
            boost::bind(
                &type::fail_on_expire,
                type::shared_from_this(),
                boost::asio::placeholders::error
            )
        );
        
        m_local_close_code = code;
        m_local_close_reason = reason;
        
        // TODO: optimize control messages and handle case where endpoint is
        // out of messages
        message::data_ptr msg = get_control_message2();
        
        if (!msg) {
            // server is out of resources, close connection.
            m_endpoint.elog().at(log::elevel::ERROR) 
            << "Server has run out of message buffers." 
            << log::endl;
            terminate(true);
            return;
        }
        
        msg->reset(frame::opcode::CLOSE);
        m_processor->prepare_close_frame(msg,code,reason);
        write_message(msg);
        
        m_write_state = INTURRUPT;
    }
    
    // send an acknowledgement close frame
    void send_close_ack() {
        // TODO: state should be OPEN
        
        // echo close value unless there is a good reason not to.
        if (m_remote_close_code == close::status::NO_STATUS) {
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
            m_endpoint.elog().at(log::elevel::ERROR) 
            << "Server has run out of message buffers." 
            << log::endl;
            terminate(true);
            return;
        }
        
        msg->reset(frame::opcode::CLOSE);
        m_processor->prepare_close_frame(msg,
                                        m_local_close_code,
                                        m_local_close_reason);
        write_message(msg);
        m_write_state = INTURRUPT;
    }
    
    void send_ping(const std::string& payload) {
        // TODO: optimize control messages and handle case where 
        // endpoint is out of messages
        message::data_ptr control = get_control_message2();
        control->reset(frame::opcode::PING);
        control->set_payload(payload);
        m_processor->prepare_frame(control);
        write_message(control);
    }
    
    void send_pong(const std::string& payload) {
        // TODO: optimize control messages and handle case where 
        // endpoint is out of messages
        message::data_ptr control = get_control_message2();
        control->reset(frame::opcode::PONG);
        control->set_payload(payload);
        m_processor->prepare_frame(control);
        write_message(control);
    }
    
    void write_message(message::data_ptr msg) {
        if (m_write_state == INTURRUPT) {
            return;
        }
        
        m_write_buffer += msg->get_payload().size();
        m_write_queue.push(msg);
        
        write();
    }
    
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
        
        if (m_write_queue.size() > 0) {
            if (m_write_state == IDLE) {
                m_write_state = WRITING;
            }
            
            //std::vector<boost::asio::const_buffer> data;
            
            
            m_write_buf.push_back(boost::asio::buffer(m_write_queue.front()->get_header()));
            m_write_buf.push_back(boost::asio::buffer(m_write_queue.front()->get_payload()));
            
            m_endpoint.alog().at(log::alevel::DEVEL) << "write header: " << to_hex(m_write_queue.front()->get_header()) << log::endl;
            
            boost::asio::async_write(
                socket_type::get_socket(),
                m_write_buf,
                //m_write_queue.front()->get_buffer(),
                boost::bind(
                    &type::handle_write,
                    type::shared_from_this(),
                    boost::asio::placeholders::error
                )
            );
        } else {
            // if we are in an inturrupted state and had nothing else to write
            // it is safe to terminate the connection.
            if (m_write_state == INTURRUPT) {
                m_endpoint.alog().at(log::alevel::DEBUG_CLOSE) 
                    << "Exit after inturrupt" << log::endl;
                terminate(false);
            }
        }
    }
    
    void handle_write(const boost::system::error_code& error) {
        if (error) {
            if (error == boost::asio::error::operation_aborted) {
                // previous write was aborted
                m_endpoint.alog().at(log::alevel::DEBUG_CLOSE) 
                    << "handle_write was called with operation_aborted error" 
                    << log::endl;
            } else {
                log_error("Error writing frame data",error);
                terminate(false);
                return;
            }
        }
        
        if (m_write_queue.size() == 0) {
            m_endpoint.alog().at(log::alevel::DEBUG_CLOSE) 
                << "handle_write called with empty queue" << log::endl;
            return;
        }
        
        m_write_buffer -= m_write_queue.front()->get_payload().size();
        m_write_buf.clear();
        m_write_queue.pop();
        
        if (m_write_state == WRITING) {
            m_write_state = IDLE;
        }
        
        write();
    }
    
    // terminate cleans up a connection and removes it from the endpoint's 
    // connection list.
    void terminate(bool failed_by_me) {
        m_endpoint.alog().at(log::alevel::DEBUG_CLOSE) 
            << "terminate called" << log::endl;
        
        if (m_state == session::state::CLOSED) {
            // shouldn't be here
        }
        
        // cancel the close timeout
        m_timer.cancel();
        
        
        
        // If this was a websocket connection notify the application handler
        // about the close using either on_fail or on_close
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
                get_handler()->on_fail(type::shared_from_this());
            } else if (old_state == session::state::OPEN || 
                       old_state == session::state::CLOSING) {
                get_handler()->on_close(type::shared_from_this());
            } else {
                // if we were already closed something is wrong
            }
            log_close_result();
        }
        // finally remove this connection from the endpoint's list. This will
        // remove the last shared pointer to the connection held by WS++.
        m_endpoint.remove_connection(type::shared_from_this());
    }
    
    // this is called when an async asio call encounters an error
    void log_error(std::string msg,const boost::system::error_code& e) {
        m_endpoint.elog().at(log::elevel::ERROR) 
        << msg << "(" << e << ")" << log::endl;
    }
    
    void log_close_result() {
        m_endpoint.alog().at(log::alevel::DISCONNECT) 
        //<< "Disconnect " << (m_was_clean ? "Clean" : "Unclean")
        << "Disconnect " 
        << " close local:[" << m_local_close_code
        << (m_local_close_reason == "" ? "" : ","+m_local_close_reason) 
        << "] remote:[" << m_remote_close_code 
        << (m_remote_close_reason == "" ? "" : ","+m_remote_close_reason) << "]"
        << log::endl;
     }
    
    void fail_on_expire(const boost::system::error_code& error) {
        if (error) {
            if (error != boost::asio::error::operation_aborted) {
                m_endpoint.elog().at(log::elevel::DEVEL) 
                << "fail_on_expire timer ended in unknown error" << log::endl;
                terminate(false);
            }
            return;
        }
        m_endpoint.elog().at(log::elevel::DEVEL) 
            << "fail_on_expire timer expired" << log::endl;
        terminate(true);
    }
    
    boost::asio::streambuf& buffer()  {
        return m_buf;
    }
    
    handler_ptr get_handler() {
        return m_handler;
    }
protected:
    endpoint_type&              m_endpoint;
    handler_ptr                 m_handler;
    
    // Network resources
    boost::asio::streambuf      m_buf;
    
    boost::asio::deadline_timer m_timer;
    
    // WebSocket connection state
    session::state::value       m_state;
    
    // stuff that actually does the work
    processor::ptr              m_processor;
    
    // Write queue
    std::vector<boost::asio::const_buffer> m_write_buf;
    std::queue<message::data_ptr>   m_write_queue;
    uint64_t                        m_write_buffer;
    write_state                     m_write_state;
    
    // Close state
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
    
} // namespace websocketpp

#endif // WEBSOCKETPP_CONNECTION_HPP
