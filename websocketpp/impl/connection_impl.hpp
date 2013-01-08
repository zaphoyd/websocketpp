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

#ifndef WEBSOCKETPP_CONNECTION_IMPL_HPP
#define WEBSOCKETPP_CONNECTION_IMPL_HPP

#include <websocketpp/common/system_error.hpp>

#include <websocketpp/processors/processor.hpp>

#include <websocketpp/processors/hybi00.hpp>
#include <websocketpp/processors/hybi07.hpp>
#include <websocketpp/processors/hybi08.hpp>
#include <websocketpp/processors/hybi13.hpp>

namespace websocketpp {

namespace istate = session::internal_state;

template <typename config>
void connection<config>::set_handler(handler_ptr new_handler) {
    std::cout << "connection set_handler" << std::endl;
    //scoped_lock_type lock(m_connection_state_lock);

    if (!new_handler) {
        // TODO
        throw std::logic_error("set_handler");
    }
    
    handler_ptr old_handler = m_handler;
    if (old_handler) {
        old_handler->on_unload(type::shared_from_this(),new_handler);
    }
    m_handler = new_handler;
    
    new_handler->on_load(type::shared_from_this(),old_handler);
}



template <typename config>
void connection<config>::set_termination_handler(
    termination_handler new_handler) 
{
    std::cout << "connection set_termination_handler" << std::endl;
    
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
void connection<config>::send(const std::string& payload, frame::opcode::value op) {
	message_ptr msg = m_msg_manager->get_message(op,payload.size());
	msg->append_payload(payload);
	
	send(msg);
}

template <typename config>
void connection<config>::send(typename config::message_type::ptr msg) {
	std::cout << "send" << std::endl;
	// TODO: 
	
    if (m_state != session::state::OPEN) {
       throw error::make_error_code(error::invalid_state);
    }
    
    message_ptr outgoing_msg;
    bool needs_writing = false;
    
    if (msg->get_prepared()) {
        outgoing_msg = msg;
        
        scoped_lock_type lock(m_write_lock);
        needs_writing = write_push(outgoing_msg);
    } else {
    	outgoing_msg = m_msg_manager->get_message();
    	
    	if (!outgoing_msg) {
    		throw error::make_error_code(error::no_outgoing_buffers);
    	}
    	
    	scoped_lock_type lock(m_write_lock);
    	lib::error_code ec = m_processor->prepare_data_frame(msg,outgoing_msg);
    	
    	if (ec) {
    		throw ec;
    	}
    	
        needs_writing = write_push(outgoing_msg);
    }
    
	if (needs_writing) {
		transport_con_type::dispatch(lib::bind(
			&type::write_frame,
			type::shared_from_this()
		));
	}
}

template <typename config>
void connection<config>::ping(const std::string& payload) {
    std::cout << "ping" << std::endl;

    if (m_state != session::state::OPEN) {
        throw error::make_error_code(error::invalid_state);
    }
    
    message_ptr msg = m_msg_manager->get_message();
    if (!msg) {
        throw error::make_error_code(error::no_outgoing_buffers);
    }

    lib::error_code ec = m_processor->prepare_ping(payload,msg);
    if (ec) {
        throw ec;
    }
    
    bool needs_writing = false;
    {
        scoped_lock_type lock(m_write_lock);
        needs_writing = write_push(msg);
    }

    if (needs_writing) {
		transport_con_type::dispatch(lib::bind(
			&type::write_frame,
			type::shared_from_this()
		));
    }
}

template <typename config>
void connection<config>::pong(const std::string& payload, lib::error_code& ec) {
    std::cout << "pong" << std::endl;

    if (m_state != session::state::OPEN) {
        ec = error::make_error_code(error::invalid_state);
        return;
    }
    
    message_ptr msg = m_msg_manager->get_message();
    if (!msg) {
        ec = error::make_error_code(error::no_outgoing_buffers);
        return;
    }

    ec = m_processor->prepare_pong(payload,msg);
    if (ec) {
        return;
    }
    
    bool needs_writing = false;
    {
        scoped_lock_type lock(m_write_lock);
        needs_writing = write_push(msg);
    }

    if (needs_writing) {
		transport_con_type::dispatch(lib::bind(
			&type::write_frame,
			type::shared_from_this()
		));
    }

    ec = lib::error_code();
}

template<typename config>
void connection<config>::pong(const std::string& payload) {
    lib::error_code ec;
    pong(payload,ec);
    if (ec) {
        throw ec;
    }
}

template <typename config>
void connection<config>::close(const close::status::value code, 
    const std::string & reason, lib::error_code & ec) 
{
    std::cout << "close" << std::endl;
    // check state
    
    // check reason length
    if (reason.size() > frame::limits::close_reason_size) {
        ec = this->send_close_frame(
            code,
            std::string(reason,0,frame::limits::close_reason_size),
            false,
            close::status::terminal(code)
        );
    } else {
        ec = this->send_close_frame(
            code,
            reason,
            false,
            close::status::terminal(code)
        );
    }
}

template<typename config>
void connection<config>::close(const close::status::value code, 
    const std::string & reason) 
{
    lib::error_code ec;
    close(code,reason,ec);
    if (ec) {
        throw ec;
    }
}

template <typename config>
lib::error_code connection<config>::interrupt() {
    std::cout << "connection::interrupt" << std::endl;
    return transport_con_type::interrupt(
        lib::bind(
            &type::handle_interrupt,
            type::shared_from_this()
        )
    );
    return lib::error_code();
}


template <typename config>
void connection<config>::handle_interrupt() {
    if (m_interrupt_handler) {
        m_interrupt_handler(m_connection_hdl);
    }
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
void connection<config>::set_status(
    http::status_code::value code) 
{
    //scoped_lock_type lock(m_connection_state_lock);
    
    if (m_internal_state != istate::PROCESS_HTTP_REQUEST) {
        throw error::make_error_code(error::invalid_state);
        //throw exception("Call to set_status from invalid state",
        //              error::INVALID_STATE);
    }
    
    m_response.set_status(code);
}
template <typename config>
void connection<config>::set_status(
    http::status_code::value code, const std::string& msg)
{
    //scoped_lock_type lock(m_connection_state_lock);
    
    if (m_internal_state != istate::PROCESS_HTTP_REQUEST) {
        throw error::make_error_code(error::invalid_state);
        //throw exception("Call to set_status from invalid state",
        //              error::INVALID_STATE);
    }
    
    m_response.set_status(code,msg);
}
template <typename config>
void connection<config>::set_body(const std::string& value) {
    //scoped_lock_type lock(m_connection_state_lock);
    
    if (m_internal_state != istate::PROCESS_HTTP_REQUEST) {
        throw error::make_error_code(error::invalid_state);
        //throw exception("Call to set_status from invalid state",
        //                error::INVALID_STATE);
    }
    
    m_response.set_body(value);
}
template <typename config>
void connection<config>::append_header(
    const std::string &key, const std::string &val)
{
    //scoped_lock_type lock(m_connection_state_lock);
    
    if (m_internal_state != istate::PROCESS_HTTP_REQUEST) {
        throw error::make_error_code(error::invalid_state);
        //throw exception("Call to set_status from invalid state",
        //                error::INVALID_STATE);
    }
    
    m_response.append_header(key,val);
}
template <typename config>
void connection<config>::replace_header(
    const std::string &key, const std::string &val)
{
   // scoped_lock_type lock(m_connection_state_lock);
    
    if (m_internal_state != istate::PROCESS_HTTP_REQUEST) {
        throw error::make_error_code(error::invalid_state);
        //throw exception("Call to set_status from invalid state",
        //                error::INVALID_STATE);
    }
    
    m_response.replace_header(key,val);
}
template <typename config>
void connection<config>::remove_header(
    const std::string &key)
{
    //scoped_lock_type lock(m_connection_state_lock);
    
    if (m_internal_state != istate::PROCESS_HTTP_REQUEST) {
        throw error::make_error_code(error::invalid_state);
        //throw exception("Call to set_status from invalid state",
        //                error::INVALID_STATE);
    }
    
    m_response.remove_header(key);
}






/******** logic thread ********/

template <typename config>
void connection<config>::start() {
    std::cout << "connection start" << std::endl;
    
    this->atomic_state_change(
        istate::USER_INIT,
        istate::TRANSPORT_INIT,
        "Start must be called from user init state"
    );
    
    // Depending on how the transport impliments init this function may return
    // immediately and call handle_transport_init later or call 
    // handle_transport_init from this function.
    transport_con_type::init(
        lib::bind(
            &type::handle_transport_init,
            type::shared_from_this(),
            lib::placeholders::_1
        )
    );
}

template <typename config>
void connection<config>::handle_transport_init(const lib::error_code& ec) {
    std::cout << "connection handle_transport_init" << std::endl;
    
    {
        scoped_lock_type lock(m_connection_state_lock);
    
        if (m_internal_state != istate::TRANSPORT_INIT) {
            throw error::make_error_code(error::invalid_state);
            //throw exception("handle_transport_init must be called from transport init state",
            //                error::INVALID_STATE);
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
    	std::cout << "handle_transport_init recieved error: "<< ec << std::endl;
        this->terminate();
        return;
    }
    
    // At this point the transport is ready to read and write bytes.
    
    if (m_is_server) {
        this->read(1);
    } else {
        // call prepare HTTP request
    }
    
    // TODO: Begin websocket handshake
    // server: read/process/write/go
    // client: process/write/read/process/go
    
    //m_handler->on_open(type::shared_from_this());
    
    //this->read();
}

template <typename config>
void connection<config>::read(size_t num_bytes) {
    std::cout << "connection read" << std::endl;
    
    transport_con_type::async_read_at_least(
        num_bytes,
        m_buf,
        config::connection_read_buffer_size,
        lib::bind(
            &type::handle_handshake_read,
            type::shared_from_this(),
            lib::placeholders::_1,
            lib::placeholders::_2
        )
    );
}

// All exit paths for this function need to call send_http_response() or submit 
// a new read request with this function as the handler.
template <typename config>
void connection<config>::handle_handshake_read(const lib::error_code& ec, 
	size_t bytes_transferred)
{
    std::cout << "connection handle_handshake_read" << std::endl;
    
    this->atomic_state_check(
        istate::READ_HTTP_REQUEST,
        "handle_handshake_read must be called from READ_HTTP_REQUEST state"
    );
    
    if (ec) {
    	std::cout << "error in handle_read_handshake: "<< ec << std::endl;
        this->terminate();
        return;
    }
            
    // Boundaries checking. TODO: How much of this should be done?
    if (bytes_transferred > config::connection_read_buffer_size) {
        std::cout << "Fatal boundaries checking error." << std::endl;
        this->terminate();
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
        std::cout << "Fatal boundaries checking error." << std::endl;
        this->terminate();
        return;
    }
    
    std::cout << "bytes_transferred: " << bytes_transferred 
              << " bytes" << std::endl;
    std::cout << "bytes_processed: " << bytes_processed 
              << " bytes" << std::endl;
    
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
                std::cout << "short key3 read" << std::endl;
                m_response.set_status(http::status_code::INTERNAL_SERVER_ERROR);
                this->send_http_response_error();
                return;
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
        this->send_http_response();
    } else {
        // read at least 1 more byte
        transport_con_type::async_read_at_least(
            1,
            m_buf,
            config::connection_read_buffer_size,
            lib::bind(
                &type::handle_handshake_read,
                type::shared_from_this(),
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
void connection<config>::handle_read_frame(const lib::error_code& ec, 
	size_t bytes_transferred)
{
    std::cout << "connection handle_read_frame" << std::endl;
    
    this->atomic_state_check(
        istate::PROCESS_CONNECTION,
        "handle_read_frame must be called from PROCESS_CONNECTION state"
    );
    
    if (ec) {
    	std::cout << "error in handle_read_frame: " << ec << std::endl;
        this->terminate();
        return;
    }
    
    // Boundaries checking. TODO: How much of this should be done?
    if (bytes_transferred > config::connection_read_buffer_size) {
        std::cout << "Fatal boundaries checking error." << std::endl;
        this->terminate();
        return;
    }
    
    size_t p = 0;
    
    std::cout << "p = " << p 
              << " bytes transferred = " << bytes_transferred << std::endl;
    
    while (p < bytes_transferred) {
		std::cout << "calling consume with " 
			      << bytes_transferred-p << " bytes" << std::endl;
        
        lib::error_code ec;

		p += m_processor->consume(
			reinterpret_cast<uint8_t*>(m_buf)+p,
			bytes_transferred-p,
            ec
		);

		std::cout << "bytes left after consume:" 
			      << bytes_transferred-p << std::endl;

		if (ec) {
			std::cout << "consume error: " << ec.message() << std::endl;
            
            if (config::drop_on_protocol_error) {
                this->terminate();
                return;
            } else {
                lib::error_code close_ec;
                this->close(processor::error::to_ws(ec),ec.message(),close_ec);

                if (close_ec) {
                    std::cout << "Failed to send a close frame after protocol error: " 
                              << close_ec.message() << std::endl;
                    this->terminate();
                    return;
                }
            }
            return;
		}

		if (m_processor->ready()) {
			std::cout << "consume ended in ready" << std::endl;
			
            message_ptr msg = m_processor->get_message();
           
            if (!msg) {
                std::cout << "null message from m_processor" << std::endl;
            } else if (!is_control(msg->get_opcode())) {
                // data message, dispatch to user
                //m_handler->on_message(type::shared_from_this(), msg);
                if (m_message_handler) {
                    m_message_handler(m_connection_hdl, msg);
                }
            } else {
                process_control_frame(msg);
            }
	    }
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
        lib::bind(
            &type::handle_read_frame,
            type::shared_from_this(),
            lib::placeholders::_1,
            lib::placeholders::_2
        )
    );
}

template <typename config>
bool connection<config>::initialize_processor() {
    std::cout << "initialize_processor" << std::endl;
    
    // if it isn't a websocket handshake nothing to do.
    if (!processor::is_websocket_handshake(m_request)) {
        return true;
    }
    
    int version = processor::get_websocket_version(m_request);
    
    if (version < 0) {
        std::cout << "BAD REQUEST: cant determine version" << std::endl;
        m_response.set_status(http::status_code::BAD_REQUEST);
        return false;
    }
    
    m_processor = get_processor(version);
    
    // if the processor is not null we are done
    if (m_processor) {
        return true;
    }

    // We don't have a processor for this version. Return bad request
    // with Sec-WebSocket-Version header filled with values we do accept
    std::cout << "BAD REQUEST: no processor for version" << std::endl;
    m_response.set_status(http::status_code::BAD_REQUEST);
    
    std::stringstream ss;
    std::string sep = "";
    std::vector<int>::const_iterator it;
    for (it = VERSIONS_SUPPORTED.begin(); it != VERSIONS_SUPPORTED.end(); it++)
    {
        ss << sep << *it;
        sep = ",";
    }
    
    m_response.replace_header("Sec-WebSocket-Version",ss.str());
    return false;
}

template <typename config>
bool connection<config>::process_handshake_request() {
    std::cout << "process handshake request" << std::endl;
    
    if (!processor::is_websocket_handshake(m_request)) {
        // this is not a websocket handshake. Process as plain HTTP
        std::cout << "HTTP REQUEST" << std::endl;

        if (m_http_handler) {
            m_http_handler(m_connection_hdl);
        }

        return true;
    }
    
    lib::error_code ec = m_processor->validate_handshake(m_request);

    // Validate: make sure all required elements are present.
    if (ec){
        // Not a valid handshake request
        std::cout << "BAD REQUEST: (724) " << ec.message() << std::endl;
        m_response.set_status(http::status_code::BAD_REQUEST);
        return false;
    }
    
    // Read extension parameters and set up values necessary for the end user
    // to complete extension negotiation.
    std::pair<lib::error_code,std::string> neg_results;
    neg_results = m_processor->negotiate_extensions(m_request);
    
    if (neg_results.first) {
        // There was a fatal error in extension parsing that should result in
        // a failed connection attempt.
        std::cout << "BAD REQUEST: (737) " << neg_results.first.message() << std::endl;
        m_response.set_status(http::status_code::BAD_REQUEST);
        return false;
    } else {
        // extension negotiation succeded, set response header accordingly
        // we don't send an empty extensions header because it breaks many
        // clients.
        if (neg_results.second.size() > 0) {
        	m_response.replace_header("Sec-WebSocket-Extensions",
            	neg_results.second);
        }
    }

    // extract URI from request
    try { 
        m_uri = m_processor->get_uri(m_request);    
    } catch (const websocketpp::uri_exception& e) {
        std::cout << "BAD REQUEST: uri failed to parse: " 
                  << e.what() << std::endl;
        m_response.set_status(http::status_code::BAD_REQUEST);
        return false;
    }
    
    // Ask application to validate the connection
    if (!m_validate_handler || m_validate_handler(m_connection_hdl)) {
        m_response.set_status(http::status_code::SWITCHING_PROTOCOLS);
        
        // Write the appropriate response headers based on request and 
        // processor version
        ec = m_processor->process_handshake(m_request,m_response);

        if (ec) {
            std::cout << "Processing error: " << ec 
                      << "(" << ec.message() << ")" << std::endl;

            m_response.set_status(http::status_code::INTERNAL_SERVER_ERROR);
            return false;
        }
    } else {
        // User application has rejected the handshake
        std::cout << "USER REJECT" << std::endl;
        
        // Use Bad Request if the user handler did not provide a more 
        // specific http response error code.
        // TODO: is there a better default?
        if (m_response.get_status_code() == http::status_code::UNINITIALIZED) {
            m_response.set_status(http::status_code::BAD_REQUEST);
        }
        
        return false;
    }
    
    return true;
}

template <typename config>
void connection<config>::handle_read(const lib::error_code& ec, 
	size_t bytes_transferred) 
{
    if (ec) {
        std::cout << "error in handle_read: " << ec << std::endl;
        return;
    }
    
    // TODO: assert bytes_transferred < m_buf size.
    
    std::cout << "connection handle_read" << std::endl;
    
    std::string foo(m_buf,bytes_transferred);
    
    // process bytes
    
    if (foo == "close") {
        this->terminate();
        return;
    }
    
    m_handler->on_message(type::shared_from_this(),foo);
    
    this->read();
}
 

template <typename config>
void connection<config>::write(std::string msg) {
    std::cout << "connection write" << std::endl;
    
    transport_con_type::async_write(
        msg.c_str(),
        msg.size(),
        lib::bind(
            &type::handle_write,
            type::shared_from_this(),
            lib::placeholders::_1
        )
    );
}
    
template <typename config>
void connection<config>::handle_write(const lib::error_code& ec) {
    if (ec) {
        std::cout << "error in handle_write: " << ec << std::endl;
        return;
    }
    
    std::cout << "connection handle_write" << std::endl;
}

template <typename config>
void connection<config>::send_http_response() {
    std::cout << "send_http_response" << std::endl;
    
    if (m_response.get_status_code() == http::status_code::UNINITIALIZED) {
        m_response.set_status(http::status_code::INTERNAL_SERVER_ERROR);
    }
    
    m_response.set_version("HTTP/1.1");
    
    // Set some common headers
    m_response.replace_header("Server",m_user_agent);
    
    std::string raw;
    
    // have the processor generate the raw bytes for the wire (if it exists)
    if (m_processor) {
        raw = m_processor->get_raw(m_response);
    } else {
        // a processor wont exist for raw HTTP responses.
        raw = m_response.raw();
    }
    
    std::cout << "Raw Handshake Response: " << std::endl;
    std::cout << raw << std::endl;
    
    // write raw bytes
    transport_con_type::async_write(
        raw.c_str(),
        raw.size(),
        lib::bind(
            &type::handle_send_http_response,
            type::shared_from_this(),
            lib::placeholders::_1
        )
    );
}

template <typename config>
void connection<config>::handle_send_http_response(
    const lib::error_code& ec)
{
    std::cout << "handle_send_http_response" << std::endl;
    
    this->atomic_state_check(
        istate::PROCESS_HTTP_REQUEST,
        "handle_send_http_response must be called from PROCESS_HTTP_REQUEST state"
    );
    
    if (ec) {
        std::cout << "error in handle_send_http_response: " << ec << std::endl;
        this->terminate();
        return;
    }
    
    if (m_response.get_status_code() != http::status_code::SWITCHING_PROTOCOLS) 
    {
        if (m_processor) {
            // if this was not a websocket connection, we have written 
            // the expected response and the connection can be closed.
        } else {
            // this was a websocket connection that ended in an error
            std::cout << "Handshake ended with HTTP error: " 
                      << m_response.get_status_code() << std::endl;
            /*m_endpoint.m_elog->at(log::elevel::RERROR) 
                << "Handshake ended with HTTP error: " 
                << m_response.get_status_code() << " " 
                << m_response.get_status_msg() << log::endl;*/
        }
        this->terminate();
        return;
    }
    
    // TODO: cancel handshake timer
    // TODO: log open result
    
    this->atomic_state_change(
        istate::PROCESS_HTTP_REQUEST,
        istate::PROCESS_CONNECTION,
        session::state::CONNECTING,
        session::state::OPEN,
        "handle_send_http_response must be called from PROCESS_HTTP_REQUEST state"
    );
    
    if (m_open_handler) {
        m_open_handler(m_connection_hdl);
    }

    this->handle_read_frame(lib::error_code(), m_buf_cursor);
}

template <typename config>
void connection<config>::terminate() {
    std::cout << "connection terminate" << std::endl;
    
    transport_con_type::shutdown();

    if (m_state == session::state::CONNECTING) {
        m_state = session::state::CLOSED;
        if (m_fail_handler) {
            m_fail_handler(m_connection_hdl);
        }
    } else {
        m_state = session::state::CLOSED;
        if (m_close_handler) {
            m_close_handler(m_connection_hdl);
        }
    }
    
    // call the termination handler if it exists
    // if it exists it might (but shouldn't) refer to a bad memory location. 
    // If it does, we don't care and should catch and ignore it.
    if (m_termination_handler) {
        try {
            m_termination_handler(type::shared_from_this());
        } catch (const std::exception& e) {
            std::cout << "termination_handler call failed. Ignoring. Reason was: " 
                      << e.what() << std::endl;
        }
    }
}

template <typename config>
void connection<config>::write_frame() {
	std::cout << "connection write_frame" << std::endl;
    
    message_ptr msg;
    {
    	scoped_lock_type lock(m_write_lock);
    	
    	if (m_send_queue.empty()) {
    		return;
    	}
    	
    	msg = write_pop();
    	
    	if (!msg) {
    		std::cout << "found empty message in write queue" << std::endl;
    		throw;
    	}
    }
    
    const std::string& header = msg->get_header();
	const std::string& payload = msg->get_payload();
    
    m_send_buffer.push_back(transport::buffer(header.c_str(),header.size()));
    m_send_buffer.push_back(transport::buffer(payload.c_str(),payload.size()));
    
    std::cout << "Dispatching write with " << header.size() 
              << " header bytes and " << payload.size() 
              << " payload bytes" << std::endl;
    std::cout << "frame is: " << utility::to_hex(header) 
              << utility::to_hex(payload) << std::endl;

    transport_con_type::async_write(
        m_send_buffer,
        lib::bind(
            &type::handle_write_frame,
            type::shared_from_this(),
            msg->get_terminal(),
            lib::placeholders::_1
        )
    );
}

template <typename config>
void connection<config>::handle_write_frame(bool terminate, 
    const lib::error_code& ec)
{
	m_send_buffer.clear();
	
	if (ec) {
        std::cout << "error in handle_write_frame: " << ec << std::endl;
        return;
    }
    
    std::cout << "connection handle_write_frame" << std::endl;
    
    if (terminate) {
        this->terminate();
        return;
    }

    bool needs_writing = false;
    {
        scoped_lock_type lock(m_write_lock);
        
        needs_writing = !m_send_queue.empty();
    }

    if (needs_writing) {
		transport_con_type::dispatch(lib::bind(
			&type::write_frame,
			type::shared_from_this()
		));
    }
}

template <typename config>
void connection<config>::atomic_state_change(istate_type req, 
    istate_type dest, std::string msg)
{
    scoped_lock_type lock(m_connection_state_lock);

    if (m_internal_state != req) {
        throw error::make_error_code(error::invalid_state);
        //throw exception(msg,error::INVALID_STATE);
    }
    
    m_internal_state = dest;
}

template <typename config>
void connection<config>::atomic_state_change(
    istate_type internal_req, istate_type internal_dest, 
    session::state::value external_req, session::state::value external_dest,
    std::string msg)
{
    scoped_lock_type lock(m_connection_state_lock);

    if (m_internal_state != internal_req || m_state != external_req) {
        throw error::make_error_code(error::invalid_state);
        //throw exception(msg,error::INVALID_STATE);
    }
    
    m_internal_state = internal_dest;
    m_state = external_dest;
}

template <typename config>
void connection<config>::atomic_state_check(istate_type req,
    std::string msg)
{
    scoped_lock_type lock(m_connection_state_lock);

    if (m_internal_state != req) {
        throw error::make_error_code(error::invalid_state);
        //throw exception(msg,error::INVALID_STATE);
    }
}

template <typename config>
const std::vector<int>& connection<config>::get_supported_versions() const
{
    return VERSIONS_SUPPORTED;
}

template <typename config>
void connection<config>::process_control_frame(typename 
    config::message_type::ptr msg)
{
    frame::opcode::value op = msg->get_opcode();
    lib::error_code ec;

    if (op == frame::opcode::PING) {
        bool pong = true;
        
        if (m_ping_handler) {
            pong = m_ping_handler(m_connection_hdl, msg->get_payload());
        }

        if (pong) {
            this->pong(msg->get_payload(),ec);
            if (ec) {
                std::cout << "Failed to send response pong: " 
                          << ec.message() << std::endl;
            }
        }
    } else if (op == frame::opcode::PONG) {
        if (m_pong_handler) {
            m_pong_handler(m_connection_hdl, msg->get_payload());
        }
    } else if (op == frame::opcode::CLOSE) {
        std::cout << "Got close frame" << std::endl;
        // record close code and reason somewhere
        
        m_remote_close_code = close::extract_code(msg->get_payload(),ec);
        if (ec) {
            if (config::drop_on_protocol_error) {
                this->terminate();
            } else {
                send_close_ack(close::status::protocol_error,
                    "Invalid close code");
            }
            return;
        }
        
        m_remote_close_reason = close::extract_reason(msg->get_payload(),ec);
        if (ec) {
            if (config::drop_on_protocol_error) {
                this->terminate();
            } else {
                send_close_ack(close::status::protocol_error,
                    "Invalid close reason");
            }
            return;
        }

        if (m_state == session::state::OPEN) {
            send_close_ack();
        } else if (m_state == session::state::CLOSING) {
            // ack of our close
            this->terminate();
        } else {
            // spurious, ignore
        }
    } else {
        // got an invalid control opcode
        // initiate protocol error shutdown
    }
}

template <typename config>
lib::error_code connection<config>::send_close_ack(close::status::value code,
    const std::string &reason)
{
    return send_close_frame(code,reason,true,m_is_server);
}

template <typename config>
lib::error_code connection<config>::send_close_frame(close::status::value code,
    const std::string &reason, bool ack, bool terminal)
{
    // If silent close is set, repsect it and blank out close information
    // Otherwise use whatever has been specified in the parameters. If
    // parameters specifies close::status::blank then determine what to do
    // based on whether or not this is an ack. If it is not an ack just
    // send blank info. If it is an ack then echo the close information from
    // the remote endpoint. 
    if (config::silent_close) {
        m_local_close_code = close::status::no_status;
        m_local_close_reason = "";
    } else if (code != close::status::blank) {
        m_local_close_code = code;
        m_local_close_reason = reason;
    } else if (!ack) {
        m_local_close_code = close::status::no_status;
        m_local_close_reason = "";
    } else if (m_remote_close_code == close::status::no_status) {
        m_local_close_code = close::status::normal;
        m_local_close_reason = "";
    } else {
        m_local_close_code = m_remote_close_code;
        m_local_close_reason = m_remote_close_reason;
    }

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
    // after the message has been written. This is typically used when clients 
    // send an ack and when any endpoint encounters a protocol error
    if (terminal) {
        msg->set_terminal(true);
    }
    
    // Concurrency review
    bool needs_writing = write_push(msg);
    
    if (needs_writing) {
		transport_con_type::dispatch(lib::bind(
			&type::write_frame,
			type::shared_from_this()
		));
    }

    return lib::error_code();
}

template <typename config>
typename connection<config>::processor_ptr
connection<config>::get_processor(int version) const {
    // TODO: allow disabling certain versions
    switch (version) {
        case 0:
            return processor_ptr(
                new processor::hybi00<config>(
                    transport_con_type::is_secure(),
                    m_is_server,
                    m_msg_manager
                )
            );
            break;
        case 7:
            return processor_ptr(
                new processor::hybi07<config>(
                    transport_con_type::is_secure(),
                    m_is_server,
                    m_msg_manager
                )
            );
            break;
        case 8:
            return processor_ptr(
                new processor::hybi08<config>(
                    transport_con_type::is_secure(),
                    m_is_server,
                    m_msg_manager
                )
            );
            break;
        case 13:
            return processor_ptr(
                new processor::hybi13<config>(
                    transport_con_type::is_secure(),
                    m_is_server,
                    m_msg_manager
                )
            );
            break;
        default:
            return processor_ptr();
    }
}

template <typename config>
bool connection<config>::write_push(typename config::message_type::ptr msg)
{
    bool empty = m_send_queue.empty();
    
    m_send_buffer_size += msg->get_payload().size();
    
    m_send_queue.push(msg);
    
    std::cout << "write_push: message count: " << m_send_queue.size()
              << " buffer size: " << m_send_buffer_size << std::endl;

    return empty;
}

template <typename config>
typename config::message_type::ptr connection<config>::write_pop()
{
    message_ptr msg = m_send_queue.front();
    
    m_send_buffer_size -= msg->get_payload().size();
    m_send_queue.pop();
    
    std::cout << "write_pop: message count: " << m_send_queue.size()
              << " buffer size: " << m_send_buffer_size << std::endl;
    return msg;
}

} // namespace websocketpp

#endif // WEBSOCKETPP_CONNECTION_IMPL_HPP
