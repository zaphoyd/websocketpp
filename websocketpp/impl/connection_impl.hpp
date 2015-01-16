/*
 * Copyright (c) 2014, Peter Thorson. All rights reserved.
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

#ifndef WEBSOCKETPP_CONNECTION_IMPL_HPP
#define WEBSOCKETPP_CONNECTION_IMPL_HPP

#include <websocketpp/common/platforms.hpp>
#include <websocketpp/common/system_error.hpp>

#include <websocketpp/processors/processor.hpp>

#include <websocketpp/processors/http11.hpp>
#include <websocketpp/processors/hybi00.hpp>
#include <websocketpp/processors/hybi07.hpp>
#include <websocketpp/processors/hybi08.hpp>
#include <websocketpp/processors/hybi13.hpp>

namespace websocketpp {

namespace istate = session::internal_state;

template <typename config>
void connection<config>::set_termination_handler(
    termination_handler new_handler)
{
    m_alog.write(log::alevel::devel,
        "connection set_termination_handler");

    //scoped_lock_type lock(m_connection_state_lock);

    m_termination_handler = new_handler;
}

template <typename config>
const std::string& connection<config>::get_origin() const {
    //scoped_lock_type lock(m_connection_state_lock);
    return m_processor->get_origin(m_request);
}

template <typename config>
size_t connection<config>::get_buffered_amount() const {
    //scoped_lock_type lock(m_connection_state_lock);
    return m_send_buffer_size;
}

template <typename config>
session::state::value connection<config>::get_state() const {
    //scoped_lock_type lock(m_connection_state_lock);
    return m_state;
}

template <typename config>
lib::error_code connection<config>::send(const std::string& payload,
    frame::opcode::value op)
{
    message_ptr msg = m_msg_manager->get_message(op,payload.size());
    msg->append_payload(payload);

    return send(msg);
}

template <typename config>
lib::error_code connection<config>::send(const void* payload, size_t len,
    frame::opcode::value op)
{
    message_ptr msg = m_msg_manager->get_message(op,len);
    msg->append_payload(payload,len);

    return send(msg);
}

template <typename config>
lib::error_code connection<config>::send(typename config::message_type::ptr msg)
{
    if (m_alog.static_test(log::alevel::devel)) {
        m_alog.write(log::alevel::devel,"connection send");
    }
    // TODO:

    if (m_state != session::state::open) {
       return error::make_error_code(error::invalid_state);
    }

    message_ptr outgoing_msg;
    bool needs_writing = false;

    if (msg->get_prepared()) {
        outgoing_msg = msg;

        scoped_lock_type lock(m_write_lock);
        write_push(outgoing_msg);
        needs_writing = !m_write_flag && !m_send_queue.empty();
    } else {
        outgoing_msg = m_msg_manager->get_message();

        if (!outgoing_msg) {
            return error::make_error_code(error::no_outgoing_buffers);
        }

        scoped_lock_type lock(m_write_lock);
        lib::error_code ec = m_processor->prepare_data_frame(msg,outgoing_msg);

        if (ec) {
            return ec;
        }

        write_push(outgoing_msg);
        needs_writing = !m_write_flag && !m_send_queue.empty();
    }

    if (needs_writing) {
        transport_con_type::dispatch(lib::bind(
            &type::write_frame,
            type::get_shared()
        ));
    }

    return lib::error_code();
}

template <typename config>
void connection<config>::ping(const std::string& payload, lib::error_code& ec) {
    if (m_alog.static_test(log::alevel::devel)) {
        m_alog.write(log::alevel::devel,"connection ping");
    }

    if (m_state != session::state::open) {
        ec = error::make_error_code(error::invalid_state);
        return;
    }

    message_ptr msg = m_msg_manager->get_message();
    if (!msg) {
        ec = error::make_error_code(error::no_outgoing_buffers);
        return;
    }

    ec = m_processor->prepare_ping(payload,msg);
    if (ec) {return;}

    // set ping timer if we are listening for one
    if (m_pong_timeout_handler) {
        // Cancel any existing timers
        if (m_ping_timer) {
            m_ping_timer->cancel();
        }

        if (m_pong_timeout_dur > 0) {
            m_ping_timer = transport_con_type::set_timer(
                m_pong_timeout_dur,
                lib::bind(
                    &type::handle_pong_timeout,
                    type::get_shared(),
                    payload,
                    lib::placeholders::_1
                )
            );
        }

        if (!m_ping_timer) {
            // Our transport doesn't support timers
            m_elog.write(log::elevel::warn,"Warning: a pong_timeout_handler is \
                set but the transport in use does not support timeouts.");
        }
    }

    bool needs_writing = false;
    {
        scoped_lock_type lock(m_write_lock);
        write_push(msg);
        needs_writing = !m_write_flag && !m_send_queue.empty();
    }

    if (needs_writing) {
        transport_con_type::dispatch(lib::bind(
            &type::write_frame,
            type::get_shared()
        ));
    }

    ec = lib::error_code();
}

template<typename config>
void connection<config>::ping(std::string const & payload) {
    lib::error_code ec;
    ping(payload,ec);
    if (ec) {
        throw exception(ec);
    }
}

template<typename config>
void connection<config>::handle_pong_timeout(std::string payload,
    lib::error_code const & ec)
{
    if (ec) {
        if (ec == transport::error::operation_aborted) {
            // ignore, this is expected
            return;
        }

        m_elog.write(log::elevel::devel,"pong_timeout error: "+ec.message());
        return;
    }

    if (m_pong_timeout_handler) {
        m_pong_timeout_handler(m_connection_hdl,payload);
    }
}

template <typename config>
void connection<config>::pong(const std::string& payload, lib::error_code& ec) {
    if (m_alog.static_test(log::alevel::devel)) {
        m_alog.write(log::alevel::devel,"connection pong");
    }

    if (m_state != session::state::open) {
        ec = error::make_error_code(error::invalid_state);
        return;
    }

    message_ptr msg = m_msg_manager->get_message();
    if (!msg) {
        ec = error::make_error_code(error::no_outgoing_buffers);
        return;
    }

    ec = m_processor->prepare_pong(payload,msg);
    if (ec) {return;}

    bool needs_writing = false;
    {
        scoped_lock_type lock(m_write_lock);
        write_push(msg);
        needs_writing = !m_write_flag && !m_send_queue.empty();
    }

    if (needs_writing) {
        transport_con_type::dispatch(lib::bind(
            &type::write_frame,
            type::get_shared()
        ));
    }

    ec = lib::error_code();
}

template<typename config>
void connection<config>::pong(std::string const & payload) {
    lib::error_code ec;
    pong(payload,ec);
    if (ec) {
        throw exception(ec);
    }
}

template <typename config>
void connection<config>::close(close::status::value const code,
    std::string const & reason, lib::error_code & ec)
{
    if (m_alog.static_test(log::alevel::devel)) {
        m_alog.write(log::alevel::devel,"connection close");
    }

    if (m_state != session::state::open) {
       ec = error::make_error_code(error::invalid_state);
       return;
    }

    // Truncate reason to maximum size allowable in a close frame.
    std::string tr(reason,0,std::min<size_t>(reason.size(),
        frame::limits::close_reason_size));

    ec = this->send_close_frame(code,tr,false,close::status::terminal(code));
}

template<typename config>
void connection<config>::close(close::status::value const code,
    std::string const & reason)
{
    lib::error_code ec;
    close(code,reason,ec);
    if (ec) {
        throw exception(ec);
    }
}

/// Trigger the on_interrupt handler
/**
 * This is thread safe if the transport is thread safe
 */
template <typename config>
lib::error_code connection<config>::interrupt() {
    m_alog.write(log::alevel::devel,"connection connection::interrupt");
    return transport_con_type::interrupt(
        lib::bind(
            &type::handle_interrupt,
            type::get_shared()
        )
    );
}


template <typename config>
void connection<config>::handle_interrupt() {
    if (m_interrupt_handler) {
        m_interrupt_handler(m_connection_hdl);
    }
}

template <typename config>
lib::error_code connection<config>::pause_reading() {
    m_alog.write(log::alevel::devel,"connection connection::pause_reading");
    return transport_con_type::dispatch(
        lib::bind(
            &type::handle_pause_reading,
            type::get_shared()
        )
    );
}

/// Pause reading handler. Not safe to call directly
template <typename config>
void connection<config>::handle_pause_reading() {
    m_alog.write(log::alevel::devel,"connection connection::handle_pause_reading");
    m_read_flag = false;
}

template <typename config>
lib::error_code connection<config>::resume_reading() {
    m_alog.write(log::alevel::devel,"connection connection::resume_reading");
    return transport_con_type::dispatch(
        lib::bind(
            &type::handle_resume_reading,
            type::get_shared()
        )
    );
}

/// Resume reading helper method. Not safe to call directly
template <typename config>
void connection<config>::handle_resume_reading() {
   m_read_flag = true;
   read_frame();
}

template <typename config>
void connection<config>::pause_http_response() {
    m_alog.write(log::alevel::devel,"connection connection::pause_http_response");
    m_http_response_paused = true;
}

template <typename config>
lib::error_code connection<config>::resume_http_response() {
    m_alog.write(log::alevel::devel,"connection connection::resume_http_response");
    return transport_con_type::dispatch(
        lib::bind(
            &type::handle_resume_http_response,
            type::get_shared()
        )
    );
}

/// Resume http_response helper method. Not safe to call directly
template <typename config>
void connection<config>::handle_resume_http_response() {
    m_http_response_paused = false;
    send_http_response();
}

template <typename config>
bool connection<config>::get_secure() const {
    //scoped_lock_type lock(m_connection_state_lock);
    return m_uri->get_secure();
}

template <typename config>
const std::string& connection<config>::get_host() const {
    //scoped_lock_type lock(m_connection_state_lock);
    return m_uri->get_host();
}

template <typename config>
const std::string& connection<config>::get_resource() const {
    //scoped_lock_type lock(m_connection_state_lock);
    return m_uri->get_resource();
}

template <typename config>
uint16_t connection<config>::get_port() const {
    //scoped_lock_type lock(m_connection_state_lock);
    return m_uri->get_port();
}

template <typename config>
uri_ptr connection<config>::get_uri() const {
    //scoped_lock_type lock(m_connection_state_lock);
    return m_uri;
}

template <typename config>
void connection<config>::set_uri(uri_ptr uri) {
    //scoped_lock_type lock(m_connection_state_lock);
    m_uri = uri;
}






template <typename config>
const std::string & connection<config>::get_subprotocol() const {
    return m_subprotocol;
}

template <typename config>
const std::vector<std::string> &
connection<config>::get_requested_subprotocols() const {
    return m_requested_subprotocols;
}

template <typename config>
void connection<config>::add_subprotocol(std::string const & value,
    lib::error_code & ec)
{
    if (m_is_server) {
        ec = error::make_error_code(error::client_only);
        return;
    }

    // If the value is empty or has a non-RFC2616 token character it is invalid.
    if (value.empty() || std::find_if(value.begin(),value.end(),
                                      http::is_not_token_char) != value.end())
    {
        ec = error::make_error_code(error::invalid_subprotocol);
        return;
    }

    m_requested_subprotocols.push_back(value);
}

template <typename config>
void connection<config>::add_subprotocol(std::string const & value) {
    lib::error_code ec;
    this->add_subprotocol(value,ec);
    if (ec) {
        throw exception(ec);
    }
}


template <typename config>
void connection<config>::select_subprotocol(std::string const & value,
    lib::error_code & ec)
{
    if (!m_is_server) {
        ec = error::make_error_code(error::server_only);
        return;
    }

    if (value.empty()) {
        ec = lib::error_code();
        return;
    }

    std::vector<std::string>::iterator it;

    it = std::find(m_requested_subprotocols.begin(),
                   m_requested_subprotocols.end(),
                   value);

    if (it == m_requested_subprotocols.end()) {
        ec = error::make_error_code(error::unrequested_subprotocol);
        return;
    }

    m_subprotocol = value;
}

template <typename config>
void connection<config>::select_subprotocol(std::string const & value) {
    lib::error_code ec;
    this->select_subprotocol(value,ec);
    if (ec) {
        throw exception(ec);
    }
}


template <typename config>
const std::string &
connection<config>::get_request_header(std::string const & key) {
    return m_request.get_header(key);
}

template <typename config>
const std::string &
connection<config>::get_request_body() const {
    return m_request.get_body();
}

template <typename config>
const std::string &
connection<config>::get_response_header(std::string const & key) {
    return m_response.get_header(key);
}

template <typename config>
const std::string &
connection<config>::get_response_body() const {
    return m_response.get_body();
}

template <typename config>
void connection<config>::set_status(http::status_code::value code)
{
    //scoped_lock_type lock(m_connection_state_lock);

    if (m_internal_state != istate::PROCESS_HTTP_REQUEST) {
        throw exception("Call to set_status from invalid state",
                      error::make_error_code(error::invalid_state));
    }

    m_response.set_status(code);
}
template <typename config>
void connection<config>::set_status(http::status_code::value code,
    std::string const & msg)
{
    //scoped_lock_type lock(m_connection_state_lock);

    if (m_internal_state != istate::PROCESS_HTTP_REQUEST) {
        throw exception("Call to set_status from invalid state",
                      error::make_error_code(error::invalid_state));
    }

    m_response.set_status(code,msg);
}
template <typename config>
void connection<config>::set_body(std::string const & value) {
    //scoped_lock_type lock(m_connection_state_lock);

    if (m_internal_state != istate::PROCESS_HTTP_REQUEST) {
        throw exception("Call to set_status from invalid state",
                      error::make_error_code(error::invalid_state));
    }

    m_response.set_body(value);
}

template <typename config>
void connection<config>::append_header(std::string const & key,
    std::string const & val)
{
    //scoped_lock_type lock(m_connection_state_lock);

    if (m_is_server) {
        if (m_internal_state == istate::PROCESS_HTTP_REQUEST) {
            // we are setting response headers for an incoming server connection
            m_response.append_header(key,val);
        } else {
            throw exception("Call to append_header from invalid state",
                      error::make_error_code(error::invalid_state));
        }
    } else {
        if (m_internal_state == istate::USER_INIT) {
            // we are setting initial headers for an outgoing client connection
            m_request.append_header(key,val);
        } else {
            throw exception("Call to append_header from invalid state",
                      error::make_error_code(error::invalid_state));
        }
    }
}
template <typename config>
void connection<config>::replace_header(std::string const & key,
    std::string const & val)
{
   // scoped_lock_type lock(m_connection_state_lock);

    if (m_is_server) {
        if (m_internal_state == istate::PROCESS_HTTP_REQUEST) {
            // we are setting response headers for an incoming server connection
            m_response.replace_header(key,val);
        } else {
            throw exception("Call to replace_header from invalid state",
                        error::make_error_code(error::invalid_state));
        }
    } else {
        if (m_internal_state == istate::USER_INIT) {
            // we are setting initial headers for an outgoing client connection
            m_request.replace_header(key,val);
        } else {
            throw exception("Call to replace_header from invalid state",
                        error::make_error_code(error::invalid_state));
        }
    }
}
template <typename config>
void connection<config>::remove_header(std::string const & key)
{
    //scoped_lock_type lock(m_connection_state_lock);

    if (m_is_server) {
        if (m_internal_state == istate::PROCESS_HTTP_REQUEST) {
            // we are setting response headers for an incoming server connection
            m_response.remove_header(key);
        } else {
            throw exception("Call to remove_header from invalid state",
                        error::make_error_code(error::invalid_state));
        }
    } else {
        if (m_internal_state == istate::USER_INIT) {
            // we are setting initial headers for an outgoing client connection
            m_request.remove_header(key);
        } else {
            throw exception("Call to remove_header from invalid state",
                        error::make_error_code(error::invalid_state));
        }
    }
}






/******** logic thread ********/

template <typename config>
void connection<config>::start() {
    m_alog.write(log::alevel::devel,"connection start");

    this->atomic_state_change(
        istate::USER_INIT,
        istate::TRANSPORT_INIT,
        "Start must be called from user init state"
    );

    // Depending on how the transport implements init this function may return
    // immediately and call handle_transport_init later or call
    // handle_transport_init from this function.
    transport_con_type::init(
        lib::bind(
            &type::handle_transport_init,
            type::get_shared(),
            lib::placeholders::_1
        )
    );
}

template <typename config>
void connection<config>::handle_transport_init(lib::error_code const & ec) {
    m_alog.write(log::alevel::devel,"connection handle_transport_init");

    {
        scoped_lock_type lock(m_connection_state_lock);

        if (m_internal_state != istate::TRANSPORT_INIT) {
            throw exception("handle_transport_init must be called from transport init state",
                            error::make_error_code(error::invalid_state));
        }

        if (!ec) {
            // unless there was a transport error, advance internal state.
            if (m_is_server) {
                m_internal_state = istate::READ_HTTP_REQUEST;
            } else {
                m_internal_state = istate::WRITE_HTTP_REQUEST;
            }
        }
    }

    if (ec) {
        std::stringstream s;
        s << "handle_transport_init received error: "<< ec.message();
        m_elog.write(log::elevel::fatal,s.str());

        this->terminate(ec);
        return;
    }

    // At this point the transport is ready to read and write bytes.
    if (m_is_server) {
        this->read_handshake(1);
    } else {
        // We are a client. Set the processor to the version specified in the
        // config file and send a handshake request.
//        m_processor = get_processor(config::client_version);
        m_processor = get_processor(m_version);
        this->send_http_request();
    }
}

template <typename config>
void connection<config>::read_handshake(size_t num_bytes) {
    m_alog.write(log::alevel::devel,"connection read");

    if (m_open_handshake_timeout_dur > 0) {
        m_handshake_timer = transport_con_type::set_timer(
            m_open_handshake_timeout_dur,
            lib::bind(
                &type::handle_open_handshake_timeout,
                type::get_shared(),
                lib::placeholders::_1
            )
        );
    }

    transport_con_type::async_read_at_least(
        num_bytes,
        m_buf,
        config::connection_read_buffer_size,
        lib::bind(
            &type::handle_read_handshake,
            type::get_shared(),
            lib::placeholders::_1,
            lib::placeholders::_2
        )
    );
}

// All exit paths for this function need to call send_http_response() or submit
// a new read request with this function as the handler.
template <typename config>
void connection<config>::handle_read_handshake(lib::error_code const & ec,
    size_t bytes_transferred)
{
    m_alog.write(log::alevel::devel,"connection handle_read_handshake");

    this->atomic_state_check(
        istate::READ_HTTP_REQUEST,
        "handle_read_handshake must be called from READ_HTTP_REQUEST state"
    );

    if (ec) {
        if (ec == transport::error::eof) {
            // we expect to get eof if the connection is closed already
            if (m_state == session::state::closed) {
                m_alog.write(log::alevel::devel,"got eof from closed con");
                return;
            }
        }

        std::stringstream s;
        s << "error in handle_read_handshake: "<< ec.message();
        m_elog.write(log::elevel::fatal,s.str());
        this->terminate(ec);
        return;
    }

    // Boundaries checking. TODO: How much of this should be done?
    if (bytes_transferred > config::connection_read_buffer_size) {
        m_elog.write(log::elevel::fatal,"Fatal boundaries checking error.");
        this->terminate(make_error_code(error::general));
        return;
    }

    size_t bytes_processed = 0;
    try {
        bytes_processed = m_request.consume(m_buf,bytes_transferred);
    } catch (http::exception &e) {
        // All HTTP exceptions will result in this request failing and an error
        // response being returned. No more bytes will be read in this con.
        m_response.set_status(e.m_error_code,e.m_error_msg);
        this->send_http_response_error();
        return;
    }

    // More paranoid boundaries checking.
    // TODO: Is this overkill?
    if (bytes_processed > config::connection_read_buffer_size) {
        m_elog.write(log::elevel::fatal,"Fatal boundaries checking error.");
        this->terminate(make_error_code(error::general));
        return;
    }

    if (m_alog.static_test(log::alevel::devel)) {
        std::stringstream s;
        s << "bytes_transferred: " << bytes_transferred
          << " bytes, bytes processed: " << bytes_processed << " bytes";
        m_alog.write(log::alevel::devel,s.str());
    }

    if (m_request.ready()) {
        if (!this->initialize_processor()) {
            this->send_http_response_error();
            return;
        }

        if (m_processor && m_processor->get_version() == 0) {
            // Version 00 has an extra requirement to read some bytes after the
            // handshake
            if (bytes_transferred-bytes_processed >= 8) {
                m_request.replace_header(
                    "Sec-WebSocket-Key3",
                    std::string(m_buf+bytes_processed,m_buf+bytes_processed+8)
                );
                bytes_processed += 8;
            } else {
                // TODO: need more bytes
                m_alog.write(log::alevel::devel,"short key3 read");
                m_response.set_status(http::status_code::internal_server_error);
                this->send_http_response_error();
                return;
            }
        }

        if (m_alog.static_test(log::alevel::devel)) {
            m_alog.write(log::alevel::devel,m_request.raw());
            if (m_request.get_header("Sec-WebSocket-Key3") != "") {
                m_alog.write(log::alevel::devel,
                    utility::to_hex(m_request.get_header("Sec-WebSocket-Key3")));
            }
        }

        // The remaining bytes in m_buf are frame data. Copy them to the
        // beginning of the buffer and note the length. They will be read after
        // the handshake completes and before more bytes are read.
        std::copy(m_buf+bytes_processed,m_buf+bytes_transferred,m_buf);
        m_buf_cursor = bytes_transferred-bytes_processed;

        this->atomic_state_change(
            istate::READ_HTTP_REQUEST,
            istate::PROCESS_HTTP_REQUEST,
            "send_http_response must be called from READ_HTTP_REQUEST state"
        );

        // We have the complete request. Process it.
        this->process_handshake_request();
        if (m_handshake_timer) {
            m_handshake_timer->cancel();
            m_handshake_timer.reset();
        }
 
        if (this->m_http_response_paused) {
           return;
        } 

        this->send_http_response();
    } else {
        // read at least 1 more byte
        transport_con_type::async_read_at_least(
            1,
            m_buf,
            config::connection_read_buffer_size,
            lib::bind(
                &type::handle_read_handshake,
                type::get_shared(),
                lib::placeholders::_1,
                lib::placeholders::_2
            )
        );
    }
}

// send_http_response requires the request to be fully read and the connection
// to be in the PROCESS_HTTP_REQUEST state. In some cases we can detect errors
// before the request is fully read (specifically at a point where we aren't
// sure if the hybi00 key3 bytes need to be read). This method sets the correct
// state and calls send_http_response
template <typename config>
void connection<config>::send_http_response_error() {
    this->atomic_state_change(
        istate::READ_HTTP_REQUEST,
        istate::PROCESS_HTTP_REQUEST,
        "send_http_response must be called from READ_HTTP_REQUEST state"
    );
    this->send_http_response();
}

// All exit paths for this function need to call send_http_response() or submit
// a new read request with this function as the handler.
template <typename config>
void connection<config>::handle_read_frame(lib::error_code const & ec,
    size_t bytes_transferred)
{
    //m_alog.write(log::alevel::devel,"connection handle_read_frame");

    this->atomic_state_check(
        istate::PROCESS_CONNECTION,
        "handle_read_frame must be called from PROCESS_CONNECTION state"
    );

    if (ec) {
        log::level echannel = log::elevel::fatal;
        
        if (ec == transport::error::eof) {
            if (m_state == session::state::closed) {
                // we expect to get eof if the connection is closed already
                // just ignore it
                m_alog.write(log::alevel::devel,"got eof from closed con");
                return;
            } else if (m_state == session::state::closing && !m_is_server) {
                // If we are a client we expect to get eof in the closing state,
                // this is a signal to terminate our end of the connection after
                // the closing handshake
                terminate(lib::error_code());
                return;
            }
        }
        if (ec == transport::error::tls_short_read) {
            if (m_state == session::state::closed) {
                // We expect to get a TLS short read if we try to read after the
                // connection is closed. If this happens ignore and exit the
                // read frame path.
                terminate(lib::error_code());
                return;
            }
            echannel = log::elevel::rerror;
        } else if (ec == transport::error::action_after_shutdown) {
            echannel = log::elevel::info;
        }
        
        log_err(echannel, "handle_read_frame", ec);
        this->terminate(ec);
        return;
    }

    // Boundaries checking. TODO: How much of this should be done?
    /*if (bytes_transferred > config::connection_read_buffer_size) {
        m_elog.write(log::elevel::fatal,"Fatal boundaries checking error");
        this->terminate(make_error_code(error::general));
        return;
    }*/

    size_t p = 0;

    if (m_alog.static_test(log::alevel::devel)) {
        std::stringstream s;
        s << "p = " << p << " bytes transferred = " << bytes_transferred;
        m_alog.write(log::alevel::devel,s.str());
    }

    while (p < bytes_transferred) {
        if (m_alog.static_test(log::alevel::devel)) {
            std::stringstream s;
            s << "calling consume with " << bytes_transferred-p << " bytes";
            m_alog.write(log::alevel::devel,s.str());
        }

        lib::error_code consume_ec;

        p += m_processor->consume(
            reinterpret_cast<uint8_t*>(m_buf)+p,
            bytes_transferred-p,
            consume_ec
        );

        if (m_alog.static_test(log::alevel::devel)) {
            std::stringstream s;
            s << "bytes left after consume: " << bytes_transferred-p;
            m_alog.write(log::alevel::devel,s.str());
        }
        if (consume_ec) {
            log_err(log::elevel::rerror, "consume", consume_ec);

            if (config::drop_on_protocol_error) {
                this->terminate(consume_ec);
                return;
            } else {
                lib::error_code close_ec;
                this->close(
                    processor::error::to_ws(consume_ec),
                    consume_ec.message(),
                    close_ec
                );

                if (close_ec) {
                    log_err(log::elevel::fatal, "Protocol error close frame ", close_ec);
                    this->terminate(close_ec);
                    return;
                }
            }
            return;
        }

        if (m_processor->ready()) {
            if (m_alog.static_test(log::alevel::devel)) {
                std::stringstream s;
                s << "Complete message received. Dispatching";
                m_alog.write(log::alevel::devel,s.str());
            }

            message_ptr msg = m_processor->get_message();

            if (!msg) {
                m_alog.write(log::alevel::devel, "null message from m_processor");
            } else if (!is_control(msg->get_opcode())) {
                // data message, dispatch to user
                if (m_state != session::state::open) {
                    m_elog.write(log::elevel::warn, "got non-close frame while closing");
                } else if (m_message_handler) {
                    m_message_handler(m_connection_hdl, msg);
                }
            } else {
                process_control_frame(msg);
            }
        }
    }

    read_frame();
}

/// Issue a new transport read unless reading is paused.
template <typename config>
void connection<config>::read_frame() {
    if (!m_read_flag) {
        return;
    }
    
    transport_con_type::async_read_at_least(
        // std::min wont work with undefined static const values.
        // TODO: is there a more elegant way to do this?
        // Need to determine if requesting 1 byte or the exact number of bytes
        // is better here. 1 byte lets us be a bit more responsive at a
        // potential expense of additional runs through handle_read_frame
        /*(m_processor->get_bytes_needed() > config::connection_read_buffer_size ?
         config::connection_read_buffer_size : m_processor->get_bytes_needed())*/
        1,
        m_buf,
        config::connection_read_buffer_size,
        m_handle_read_frame
    );
}

template <typename config>
bool connection<config>::initialize_processor() {
    m_alog.write(log::alevel::devel,"initialize_processor");

    // if it isn't a websocket handshake nothing to do.
    if (!processor::is_websocket_handshake(m_request)) {
        return true;
    }

    int version = processor::get_websocket_version(m_request);

    if (version < 0) {
        m_alog.write(log::alevel::devel, "BAD REQUEST: can't determine version");
        m_response.set_status(http::status_code::bad_request);
        return false;
    }

    m_processor = get_processor(version);

    // if the processor is not null we are done
    if (m_processor) {
        return true;
    }

    // We don't have a processor for this version. Return bad request
    // with Sec-WebSocket-Version header filled with values we do accept
    m_alog.write(log::alevel::devel, "BAD REQUEST: no processor for version");
    m_response.set_status(http::status_code::bad_request);

    std::stringstream ss;
    std::string sep = "";
    std::vector<int>::const_iterator it;
    for (it = versions_supported.begin(); it != versions_supported.end(); it++)
    {
        ss << sep << *it;
        sep = ",";
    }

    m_response.replace_header("Sec-WebSocket-Version",ss.str());
    return false;
}

template <typename config>
bool connection<config>::process_handshake_request() {
    m_alog.write(log::alevel::devel,"process handshake request");

    if (!processor::is_websocket_handshake(m_request)) {
        // this is not a websocket handshake. Process as plain HTTP
        m_alog.write(log::alevel::devel,"HTTP REQUEST");

        // extract URI from request
        m_uri = processor::get_uri_from_host(
            m_request,
            (transport_con_type::is_secure() ? "https" : "http")
        );

        if (!m_uri->get_valid()) {
            m_alog.write(log::alevel::devel, "Bad request: failed to parse uri");
            m_response.set_status(http::status_code::bad_request);
            return false;
        }

        if (m_http_handler) {
            m_http_handler(m_connection_hdl);
        } else {
            set_status(http::status_code::upgrade_required);
        }

        return true;
    }

    lib::error_code ec = m_processor->validate_handshake(m_request);

    // Validate: make sure all required elements are present.
    if (ec){
        // Not a valid handshake request
        m_alog.write(log::alevel::devel, "Bad request " + ec.message());
        m_response.set_status(http::status_code::bad_request);
        return false;
    }

    // Read extension parameters and set up values necessary for the end user
    // to complete extension negotiation.
    std::pair<lib::error_code,std::string> neg_results;
    neg_results = m_processor->negotiate_extensions(m_request);

    if (neg_results.first) {
        // There was a fatal error in extension parsing that should result in
        // a failed connection attempt.
        m_alog.write(log::alevel::devel, "Bad request: " + neg_results.first.message());
        m_response.set_status(http::status_code::bad_request);
        return false;
    } else {
        // extension negotiation succeeded, set response header accordingly
        // we don't send an empty extensions header because it breaks many
        // clients.
        if (neg_results.second.size() > 0) {
            m_response.replace_header("Sec-WebSocket-Extensions",
                neg_results.second);
        }
    }

    // extract URI from request
    m_uri = m_processor->get_uri(m_request);


    if (!m_uri->get_valid()) {
        m_alog.write(log::alevel::devel, "Bad request: failed to parse uri");
        m_response.set_status(http::status_code::bad_request);
        return false;
    }

    // extract subprotocols
    lib::error_code subp_ec = m_processor->extract_subprotocols(m_request,
        m_requested_subprotocols);

    if (subp_ec) {
        // should we do anything?
    }

    // Ask application to validate the connection
    if (!m_validate_handler || m_validate_handler(m_connection_hdl)) {
        m_response.set_status(http::status_code::switching_protocols);

        // Write the appropriate response headers based on request and
        // processor version
        ec = m_processor->process_handshake(m_request,m_subprotocol,m_response);

        if (ec) {
            std::stringstream s;
            s << "Processing error: " << ec << "(" << ec.message() << ")";
            m_alog.write(log::alevel::devel, s.str());

            m_response.set_status(http::status_code::internal_server_error);
            return false;
        }
    } else {
        // User application has rejected the handshake
        m_alog.write(log::alevel::devel, "USER REJECT");

        // Use Bad Request if the user handler did not provide a more
        // specific http response error code.
        // TODO: is there a better default?
        if (m_response.get_status_code() == http::status_code::uninitialized) {
            m_response.set_status(http::status_code::bad_request);
        }

        return false;
    }

    return true;
}

template <typename config>
void connection<config>::send_http_response() {
    m_alog.write(log::alevel::devel,"connection send_http_response");

    if (m_response.get_status_code() == http::status_code::uninitialized) {
        m_response.set_status(http::status_code::internal_server_error);
    }

    m_response.set_version("HTTP/1.1");

    // Set server header based on the user agent settings
    if (m_response.get_header("Server") == "") {
        if (!m_user_agent.empty()) {
            m_response.replace_header("Server",m_user_agent);
        } else {
            m_response.remove_header("Server");
        }
    }

    // have the processor generate the raw bytes for the wire (if it exists)
    if (m_processor) {
        m_handshake_buffer = m_processor->get_raw(m_response);
    } else {
        // a processor wont exist for raw HTTP responses.
        m_handshake_buffer = m_response.raw();
    }

    if (m_alog.static_test(log::alevel::devel)) {
        m_alog.write(log::alevel::devel,"Raw Handshake response:\n"+m_handshake_buffer);
        if (m_response.get_header("Sec-WebSocket-Key3") != "") {
            m_alog.write(log::alevel::devel,
                utility::to_hex(m_response.get_header("Sec-WebSocket-Key3")));
        }
    }

    // write raw bytes
    transport_con_type::async_write(
        m_handshake_buffer.data(),
        m_handshake_buffer.size(),
        lib::bind(
            &type::handle_send_http_response,
            type::get_shared(),
            lib::placeholders::_1
        )
    );
}

template <typename config>
void connection<config>::handle_send_http_response(lib::error_code const & ec) {
    m_alog.write(log::alevel::devel,"handle_send_http_response");

    this->atomic_state_check(
        istate::PROCESS_HTTP_REQUEST,
        "handle_send_http_response must be called from PROCESS_HTTP_REQUEST state"
    );

    if (ec) {
        log_err(log::elevel::rerror,"handle_send_http_response",ec);
        this->terminate(ec);
        return;
    }

    this->log_open_result();

    if (m_handshake_timer) {
        m_handshake_timer->cancel();
        m_handshake_timer.reset();
    }

    if (m_response.get_status_code() != http::status_code::switching_protocols)
    {
        if (m_processor) {
            // this was a websocket connection that ended in an error
            std::stringstream s;
            s << "Handshake ended with HTTP error: "
              << m_response.get_status_code();
            m_elog.write(log::elevel::rerror,s.str());
        } else {
            // if this was not a websocket connection, we have written
            // the expected response and the connection can be closed.
        }
        this->terminate(make_error_code(error::http_connection_ended));
        return;
    }

    this->atomic_state_change(
        istate::PROCESS_HTTP_REQUEST,
        istate::PROCESS_CONNECTION,
        session::state::connecting,
        session::state::open,
        "handle_send_http_response must be called from PROCESS_HTTP_REQUEST state"
    );

    if (m_open_handler) {
        m_open_handler(m_connection_hdl);
    }

    this->handle_read_frame(lib::error_code(), m_buf_cursor);
}

template <typename config>
void connection<config>::send_http_request() {
    m_alog.write(log::alevel::devel,"connection send_http_request");

    // TODO: origin header?

    // Have the protocol processor fill in the appropriate fields based on the
    // selected client version
    if (m_processor) {
        lib::error_code ec;
        ec = m_processor->client_handshake_request(m_request,m_uri,
            m_requested_subprotocols);

        if (ec) {
            log_err(log::elevel::fatal,"Internal library error: Processor",ec);
            return;
        }
    } else {
        m_elog.write(log::elevel::fatal,"Internal library error: missing processor");
        return;
    }

    // Unless the user has overridden the user agent, send generic WS++ UA.
    if (m_request.get_header("User-Agent") == "") {
        if (!m_user_agent.empty()) {
            m_request.replace_header("User-Agent",m_user_agent);
        } else {
            m_request.remove_header("User-Agent");
        }
    }

    m_handshake_buffer = m_request.raw();

    if (m_alog.static_test(log::alevel::devel)) {
        m_alog.write(log::alevel::devel,"Raw Handshake request:\n"+m_handshake_buffer);
    }

    if (m_open_handshake_timeout_dur > 0) {
        m_handshake_timer = transport_con_type::set_timer(
            m_open_handshake_timeout_dur,
            lib::bind(
                &type::handle_open_handshake_timeout,
                type::get_shared(),
                lib::placeholders::_1
            )
        );
    }

    transport_con_type::async_write(
        m_handshake_buffer.data(),
        m_handshake_buffer.size(),
        lib::bind(
            &type::handle_send_http_request,
            type::get_shared(),
            lib::placeholders::_1
        )
    );
}

template <typename config>
void connection<config>::handle_send_http_request(lib::error_code const & ec) {
    m_alog.write(log::alevel::devel,"handle_send_http_request");

    this->atomic_state_check(
        istate::WRITE_HTTP_REQUEST,
        "handle_send_http_request must be called from WRITE_HTTP_REQUEST state"
    );

    if (ec) {
        log_err(log::elevel::rerror,"handle_send_http_request",ec);
        this->terminate(ec);
        return;
    }

    this->atomic_state_change(
        istate::WRITE_HTTP_REQUEST,
        istate::READ_HTTP_RESPONSE,
        "handle_send_http_request must be called from WRITE_HTTP_REQUEST state"
    );

    transport_con_type::async_read_at_least(
        1,
        m_buf,
        config::connection_read_buffer_size,
        lib::bind(
            &type::handle_read_http_response,
            type::get_shared(),
            lib::placeholders::_1,
            lib::placeholders::_2
        )
    );
}

template <typename config>
void connection<config>::handle_read_http_response(lib::error_code const & ec,
    size_t bytes_transferred)
{
    m_alog.write(log::alevel::devel,"handle_read_http_response");

    this->atomic_state_check(
        istate::READ_HTTP_RESPONSE,
        "handle_read_http_response must be called from READ_HTTP_RESPONSE state"
    );

    if (ec) {
        if (ec == websocketpp::transport::error::eof) {
          m_response.consume(m_buf,0);
          if (m_message_handler) {
            m_message_handler(m_connection_hdl, nullptr);
          }
        }

        log_err(log::elevel::rerror,"handle_send_http_response",ec);
        this->terminate(ec);
        return;
    }
    size_t bytes_processed = 0;
    // TODO: refactor this to use error codes rather than exceptions
    try {
        bytes_processed = m_response.consume(m_buf,bytes_transferred);
    } catch (http::exception & e) {
        m_elog.write(log::elevel::rerror,
            std::string("error in handle_read_http_response: ")+e.what());
        this->terminate(make_error_code(error::general));
        return;
    }

    m_alog.write(log::alevel::devel,std::string("Raw response: ")+m_response.raw());

    if (m_response.headers_ready() && m_processor->is_websocket()) {
        if (m_handshake_timer) {
            m_handshake_timer->cancel();
            m_handshake_timer.reset();
        }

        lib::error_code validate_ec = m_processor->validate_server_handshake_response(
            m_request,
            m_response
        );
        if (validate_ec) {
            log_err(log::elevel::rerror,"Server handshake response",validate_ec);
            this->terminate(validate_ec);
            return;
        }

        // response is valid, connection can now be assumed to be open
        this->atomic_state_change(
            istate::READ_HTTP_RESPONSE,
            istate::PROCESS_CONNECTION,
            session::state::connecting,
            session::state::open,
            "handle_read_http_response must be called from READ_HTTP_RESPONSE state"
        );

        this->log_open_result();

        if (m_open_handler) {
            m_open_handler(m_connection_hdl);
        }

        // The remaining bytes in m_buf are frame data. Copy them to the
        // beginning of the buffer and note the length. They will be read after
        // the handshake completes and before more bytes are read.
        std::copy(m_buf+bytes_processed,m_buf+bytes_transferred,m_buf);
        m_buf_cursor = bytes_transferred-bytes_processed;

        this->handle_read_frame(lib::error_code(), m_buf_cursor);
    } else {
        if (m_response.ready()) {
          if (m_message_handler) {
            m_message_handler(m_connection_hdl, nullptr);
          }
        }
        else { 
          transport_con_type::async_read_at_least(
            1,
            m_buf,
            config::connection_read_buffer_size,
            lib::bind(
                &type::handle_read_http_response,
                type::get_shared(),
                lib::placeholders::_1,
                lib::placeholders::_2
            )
          );
        }
    }
}

template <typename config>
void connection<config>::handle_open_handshake_timeout(
    lib::error_code const & ec)
{
    if (ec == transport::error::operation_aborted) {
        m_alog.write(log::alevel::devel,"open handshake timer cancelled");
    } else if (ec) {
        m_alog.write(log::alevel::devel,
            "open handle_open_handshake_timeout error: "+ec.message());
        // TODO: ignore or fail here?
    } else {
        m_alog.write(log::alevel::devel,"open handshake timer expired");
        terminate(make_error_code(error::open_handshake_timeout));
    }
}

template <typename config>
void connection<config>::handle_close_handshake_timeout(
    lib::error_code const & ec)
{
    if (ec == transport::error::operation_aborted) {
        m_alog.write(log::alevel::devel,"asio close handshake timer cancelled");
    } else if (ec) {
        m_alog.write(log::alevel::devel,
            "asio open handle_close_handshake_timeout error: "+ec.message());
        // TODO: ignore or fail here?
    } else {
        m_alog.write(log::alevel::devel, "asio close handshake timer expired");
        terminate(make_error_code(error::close_handshake_timeout));
    }
}

template <typename config>
void connection<config>::terminate(lib::error_code const & ec) {
    if (m_alog.static_test(log::alevel::devel)) {
        m_alog.write(log::alevel::devel,"connection terminate");
    }

    // Cancel close handshake timer
    if (m_handshake_timer) {
        m_handshake_timer->cancel();
        m_handshake_timer.reset();
    }

    terminate_status tstat = unknown;
    if (ec) {
        m_ec = ec;
        m_local_close_code = close::status::abnormal_close;
        m_local_close_reason = ec.message();
    }

    if (m_state == session::state::connecting) {
        m_state = session::state::closed;
        tstat = failed;
    } else if (m_state != session::state::closed) {
        m_state = session::state::closed;
        tstat = closed;
    } else {
        m_alog.write(log::alevel::devel,
            "terminate called on connection that was already terminated");
        return;
    }

    // TODO: choose between shutdown and close based on error code sent

    transport_con_type::async_shutdown(
        lib::bind(
            &type::handle_terminate,
            type::get_shared(),
            tstat,
            lib::placeholders::_1
        )
    );
}

template <typename config>
void connection<config>::handle_terminate(terminate_status tstat,
    lib::error_code const & ec)
{
    if (m_alog.static_test(log::alevel::devel)) {
        m_alog.write(log::alevel::devel,"connection handle_terminate");
    }

    if (ec) {
        // there was an error actually shutting down the connection
        log_err(log::elevel::devel,"handle_terminate",ec);
    }

    // clean shutdown
    if (tstat == failed) {
        if (m_fail_handler) {
            m_fail_handler(m_connection_hdl);
        }
        log_fail_result();
    } else if (tstat == closed) {
        if (m_close_handler) {
            m_close_handler(m_connection_hdl);
        }
        log_close_result();
    } else {
        m_elog.write(log::elevel::rerror,"Unknown terminate_status");
    }

    // call the termination handler if it exists
    // if it exists it might (but shouldn't) refer to a bad memory location.
    // If it does, we don't care and should catch and ignore it.
    if (m_termination_handler) {
        try {
            m_termination_handler(type::get_shared());
        } catch (std::exception const & e) {
            m_elog.write(log::elevel::warn,
                std::string("termination_handler call failed. Reason was: ")+e.what());
        }
    }
}

template <typename config>
void connection<config>::write_frame() {
    //m_alog.write(log::alevel::devel,"connection write_frame");

    {
        scoped_lock_type lock(m_write_lock);

        // Check the write flag. If true, there is an outstanding transport
        // write already. In this case we just return. The write handler will
        // start a new write if the write queue isn't empty. If false, we set
        // the write flag and proceed to initiate a transport write.
        if (m_write_flag) {
            return;
        }

        // pull off all the messages that are ready to write.
        // stop if we get a message marked terminal
        message_ptr next_message = write_pop();
        while (next_message) {
            m_current_msgs.push_back(next_message);
            if (!next_message->get_terminal()) {
                next_message = write_pop();
            } else {
                next_message = message_ptr();
            }
        }
        
        if (m_current_msgs.empty()) {
            // there was nothing to send
            return;
        } else {
            // At this point we own the next messages to be sent and are
            // responsible for holding the write flag until they are 
            // successfully sent or there is some error
            m_write_flag = true;
        }
    }

    typename std::vector<message_ptr>::iterator it;
    for (it = m_current_msgs.begin(); it != m_current_msgs.end(); ++it) {
        std::string const & header = (*it)->get_header();
        std::string const & payload = (*it)->get_payload();

        m_send_buffer.push_back(transport::buffer(header.c_str(),header.size()));
        m_send_buffer.push_back(transport::buffer(payload.c_str(),payload.size()));   
    }

    // Print detailed send stats if those log levels are enabled
    if (m_alog.static_test(log::alevel::frame_header)) {
    if (m_alog.dynamic_test(log::alevel::frame_header)) {
        std::stringstream general,header,payload;
        
        general << "Dispatching write containing " << m_current_msgs.size()
                <<" message(s) containing ";
        header << "Header Bytes: \n";
        payload << "Payload Bytes: \n";
        
        size_t hbytes = 0;
        size_t pbytes = 0;
        
        for (size_t i = 0; i < m_current_msgs.size(); i++) {
            hbytes += m_current_msgs[i]->get_header().size();
            pbytes += m_current_msgs[i]->get_payload().size();

            
            header << "[" << i << "] (" 
                   << m_current_msgs[i]->get_header().size() << ") " 
                   << utility::to_hex(m_current_msgs[i]->get_header()) << "\n";

            if (m_alog.static_test(log::alevel::frame_payload)) {
            if (m_alog.dynamic_test(log::alevel::frame_payload)) {
                payload << "[" << i << "] (" 
                        << m_current_msgs[i]->get_payload().size() << ") " 
                        << utility::to_hex(m_current_msgs[i]->get_payload()) 
                        << "\n";
            }
            }  
        }
        
        general << hbytes << " header bytes and " << pbytes << " payload bytes";
        
        m_alog.write(log::alevel::frame_header,general.str());
        m_alog.write(log::alevel::frame_header,header.str());
        m_alog.write(log::alevel::frame_payload,payload.str());
    }
    }

    transport_con_type::async_write(
        m_send_buffer,
        m_write_frame_handler
    );
}

template <typename config>
void connection<config>::handle_write_frame(lib::error_code const & ec)
{
    if (m_alog.static_test(log::alevel::devel)) {
        m_alog.write(log::alevel::devel,"connection handle_write_frame");
    }

    bool terminal = m_current_msgs.back()->get_terminal();

    m_send_buffer.clear();
    m_current_msgs.clear();
    // TODO: recycle instead of deleting

    if (ec) {
        log_err(log::elevel::fatal,"handle_write_frame",ec);
        this->terminate(ec);
        return;
    }

    if (terminal) {
        this->terminate(lib::error_code());
        return;
    }

    bool needs_writing = false;
    {
        scoped_lock_type lock(m_write_lock);

        // release write flag
        m_write_flag = false;

        needs_writing = !m_send_queue.empty();
    }

    if (needs_writing) {
        transport_con_type::dispatch(lib::bind(
            &type::write_frame,
            type::get_shared()
        ));
    }
}

template <typename config>
void connection<config>::atomic_state_change(istate_type req, istate_type dest,
    std::string msg)
{
    scoped_lock_type lock(m_connection_state_lock);

    if (m_internal_state != req) {
        throw exception(msg,error::make_error_code(error::invalid_state));
    }

    m_internal_state = dest;
}

template <typename config>
void connection<config>::atomic_state_change(istate_type internal_req, 
    istate_type internal_dest, session::state::value external_req, 
    session::state::value external_dest, std::string msg)
{
    scoped_lock_type lock(m_connection_state_lock);

    if (m_internal_state != internal_req || m_state != external_req) {
        throw exception(msg,error::make_error_code(error::invalid_state));
    }

    m_internal_state = internal_dest;
    m_state = external_dest;
}

template <typename config>
void connection<config>::atomic_state_check(istate_type req, std::string msg)
{
    scoped_lock_type lock(m_connection_state_lock);

    if (m_internal_state != req) {
        throw exception(msg,error::make_error_code(error::invalid_state));
    }
}

template <typename config>
const std::vector<int>& connection<config>::get_supported_versions() const
{
    return versions_supported;
}

template <typename config>
void connection<config>::process_control_frame(typename config::message_type::ptr msg)
{
    m_alog.write(log::alevel::devel,"process_control_frame");

    frame::opcode::value op = msg->get_opcode();
    lib::error_code ec;

    std::stringstream s;
    s << "Control frame received with opcode " << op;
    m_alog.write(log::alevel::control,s.str());

    if (m_state == session::state::closed) {
        m_elog.write(log::elevel::warn,"got frame in state closed");
        return;
    }
    if (op != frame::opcode::CLOSE && m_state != session::state::open) {
        m_elog.write(log::elevel::warn,"got non-close frame in state closing");
        return;
    }

    if (op == frame::opcode::PING) {
        bool should_reply = true;

        if (m_ping_handler) {
            should_reply = m_ping_handler(m_connection_hdl, msg->get_payload());
        }

        if (should_reply) {
            this->pong(msg->get_payload(),ec);
            if (ec) {
                log_err(log::elevel::devel,"Failed to send response pong",ec);
            }
        }
    } else if (op == frame::opcode::PONG) {
        if (m_pong_handler) {
            m_pong_handler(m_connection_hdl, msg->get_payload());
        }
        if (m_ping_timer) {
            m_ping_timer->cancel();
        }
    } else if (op == frame::opcode::CLOSE) {
        m_alog.write(log::alevel::devel,"got close frame");
        // record close code and reason somewhere

        m_remote_close_code = close::extract_code(msg->get_payload(),ec);
        if (ec) {
            s.str("");
            if (config::drop_on_protocol_error) {
                s << "Received invalid close code " << m_remote_close_code
                  << " dropping connection per config.";
                m_elog.write(log::elevel::devel,s.str());
                this->terminate(ec);
            } else {
                s << "Received invalid close code " << m_remote_close_code
                  << " sending acknowledgement and closing";
                m_elog.write(log::elevel::devel,s.str());
                ec = send_close_ack(close::status::protocol_error,
                    "Invalid close code");
                if (ec) {
                    log_err(log::elevel::devel,"send_close_ack",ec);
                }
            }
            return;
        }

        m_remote_close_reason = close::extract_reason(msg->get_payload(),ec);
        if (ec) {
            if (config::drop_on_protocol_error) {
                m_elog.write(log::elevel::devel,
                    "Received invalid close reason. Dropping connection per config");
                this->terminate(ec);
            } else {
                m_elog.write(log::elevel::devel,
                    "Received invalid close reason. Sending acknowledgement and closing");
                ec = send_close_ack(close::status::protocol_error,
                    "Invalid close reason");
                if (ec) {
                    log_err(log::elevel::devel,"send_close_ack",ec);
                }
            }
            return;
        }

        if (m_state == session::state::open) {
            s.str("");
            s << "Received close frame with code " << m_remote_close_code
              << " and reason " << m_remote_close_reason;
            m_alog.write(log::alevel::devel,s.str());

            ec = send_close_ack();
            if (ec) {
                log_err(log::elevel::devel,"send_close_ack",ec);
            }
        } else if (m_state == session::state::closing && !m_was_clean) {
            // ack of our close
            m_alog.write(log::alevel::devel, "Got acknowledgement of close");

            m_was_clean = true;

            // If we are a server terminate the connection now. Clients should
            // leave the connection open to give the server an opportunity to
            // initiate the TCP close. The client's timer will handle closing
            // its side of the connection if the server misbehaves.
            //
            // TODO: different behavior if the underlying transport doesn't
            // support timers?
            if (m_is_server) {
                terminate(lib::error_code());
            }
        } else {
            // spurious, ignore
            m_elog.write(log::elevel::devel, "Got close frame in wrong state");
        }
    } else {
        // got an invalid control opcode
        m_elog.write(log::elevel::devel, "Got control frame with invalid opcode");
        // initiate protocol error shutdown
    }
}

template <typename config>
lib::error_code connection<config>::send_close_ack(close::status::value code,
    std::string const & reason)
{
    return send_close_frame(code,reason,true,m_is_server);
}

template <typename config>
lib::error_code connection<config>::send_close_frame(close::status::value code,
    std::string const & reason, bool ack, bool terminal)
{
    m_alog.write(log::alevel::devel,"send_close_frame");

    // check for special codes

    // If silent close is set, respect it and blank out close information
    // Otherwise use whatever has been specified in the parameters. If
    // parameters specifies close::status::blank then determine what to do
    // based on whether or not this is an ack. If it is not an ack just
    // send blank info. If it is an ack then echo the close information from
    // the remote endpoint.
    if (config::silent_close) {
        m_alog.write(log::alevel::devel,"closing silently");
        m_local_close_code = close::status::no_status;
        m_local_close_reason = "";
    } else if (code != close::status::blank) {
        m_alog.write(log::alevel::devel,"closing with specified codes");
        m_local_close_code = code;
        m_local_close_reason = reason;
    } else if (!ack) {
        m_alog.write(log::alevel::devel,"closing with no status code");
        m_local_close_code = close::status::no_status;
        m_local_close_reason = "";
    } else if (m_remote_close_code == close::status::no_status) {
        m_alog.write(log::alevel::devel,
            "acknowledging a no-status close with normal code");
        m_local_close_code = close::status::normal;
        m_local_close_reason = "";
    } else {
        m_alog.write(log::alevel::devel,"acknowledging with remote codes");
        m_local_close_code = m_remote_close_code;
        m_local_close_reason = m_remote_close_reason;
    }

    std::stringstream s;
    s << "Closing with code: " << m_local_close_code << ", and reason: "
      << m_local_close_reason;
    m_alog.write(log::alevel::devel,s.str());

    message_ptr msg = m_msg_manager->get_message();
    if (!msg) {
        return error::make_error_code(error::no_outgoing_buffers);
    }

    lib::error_code ec = m_processor->prepare_close(m_local_close_code,
        m_local_close_reason,msg);
    if (ec) {
        return ec;
    }

    // Messages flagged terminal will result in the TCP connection being dropped
    // after the message has been written. This is typically used when servers
    // send an ack and when any endpoint encounters a protocol error
    if (terminal) {
        msg->set_terminal(true);
    }

    m_state = session::state::closing;

    if (ack) {
        m_was_clean = true;
    }

    // Start a timer so we don't wait forever for the acknowledgement close
    // frame
    if (m_close_handshake_timeout_dur > 0) {
        m_handshake_timer = transport_con_type::set_timer(
            m_close_handshake_timeout_dur,
            lib::bind(
                &type::handle_close_handshake_timeout,
                type::get_shared(),
                lib::placeholders::_1
            )
        );
    }

    bool needs_writing = false;
    {
        scoped_lock_type lock(m_write_lock);
        write_push(msg);
        needs_writing = !m_write_flag && !m_send_queue.empty();
    }

    if (needs_writing) {
        transport_con_type::dispatch(lib::bind(
            &type::write_frame,
            type::get_shared()
        ));
    }

    return lib::error_code();
}

template <typename config>
typename connection<config>::processor_ptr
connection<config>::get_processor(int version) const {
    // TODO: allow disabling certain versions
    
    processor_ptr p;
    
    switch (version) {
        case -1:
            p.reset(
                new processor::http11<config>(
                    transport_con_type::is_secure(),
                    m_is_server,
                    m_msg_manager
                )
            );
            return p;
 
        case 0:
            p = lib::make_shared<processor::hybi00<config> >(
                transport_con_type::is_secure(),
                m_is_server,
                m_msg_manager
            );
            break;
        case 7:
            p = lib::make_shared<processor::hybi07<config> >(
                transport_con_type::is_secure(),
                m_is_server,
                m_msg_manager,
                lib::ref(m_rng)
            );
            break;
        case 8:
            p = lib::make_shared<processor::hybi08<config> >(
                transport_con_type::is_secure(),
                m_is_server,
                m_msg_manager,
                lib::ref(m_rng)
            );
            break;
        case 13:
            p = lib::make_shared<processor::hybi13<config> >(
                transport_con_type::is_secure(),
                m_is_server,
                m_msg_manager,
                lib::ref(m_rng)
            );
            break;
        default:
            return p;
    }
    
    // Settings not configured by the constructor
    p->set_max_message_size(m_max_message_size);
    
    return p;
}

template <typename config>
void connection<config>::write_push(typename config::message_type::ptr msg)
{
    if (!msg) {
        return;
    }

    m_send_buffer_size += msg->get_payload().size();
    m_send_queue.push(msg);

    if (m_alog.static_test(log::alevel::devel)) {
        std::stringstream s;
        s << "write_push: message count: " << m_send_queue.size()
          << " buffer size: " << m_send_buffer_size;
        m_alog.write(log::alevel::devel,s.str());
    }
}

template <typename config>
typename config::message_type::ptr connection<config>::write_pop()
{
    message_ptr msg;

    if (m_send_queue.empty()) {
        return msg;
    }

    msg = m_send_queue.front();

    m_send_buffer_size -= msg->get_payload().size();
    m_send_queue.pop();

    if (m_alog.static_test(log::alevel::devel)) {
        std::stringstream s;
        s << "write_pop: message count: " << m_send_queue.size()
          << " buffer size: " << m_send_buffer_size;
        m_alog.write(log::alevel::devel,s.str());
    }
    return msg;
}

template <typename config>
void connection<config>::log_open_result()
{
    std::stringstream s;

    int version;
    if (!processor::is_websocket_handshake(m_request)) {
        version = -1;
    } else {
        version = processor::get_websocket_version(m_request);
    }

    // Connection Type
    s << (version == -1 ? "HTTP" : "WebSocket") << " Connection ";

    // Remote endpoint address
    s << transport_con_type::get_remote_endpoint() << " ";

    // Version string if WebSocket
    if (version != -1) {
        s << "v" << version << " ";
    }

    // User Agent
    std::string ua = m_request.get_header("User-Agent");
    if (ua == "") {
        s << "\"\" ";
    } else {
        // check if there are any quotes in the user agent
        s << "\"" << utility::string_replace_all(ua,"\"","\\\"") << "\" ";
    }

    // URI
    s << (m_uri ? m_uri->get_resource() : "NULL") << " ";

    // Status code
    s << m_response.get_status_code();

    m_alog.write(log::alevel::connect,s.str());
}

template <typename config>
void connection<config>::log_close_result()
{
    std::stringstream s;

    s << "Disconnect "
      << "close local:[" << m_local_close_code
      << (m_local_close_reason == "" ? "" : ","+m_local_close_reason)
      << "] remote:[" << m_remote_close_code
      << (m_remote_close_reason == "" ? "" : ","+m_remote_close_reason) << "]";

    m_alog.write(log::alevel::disconnect,s.str());
}

template <typename config>
void connection<config>::log_fail_result()
{
    // TODO: include more information about the connection?
    // should this be filed under connect rather than disconnect?
    m_alog.write(log::alevel::disconnect,"Failed: "+m_ec.message());
}

} // namespace websocketpp

#endif // WEBSOCKETPP_CONNECTION_IMPL_HPP
