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

#ifndef WEBSOCKET_PROCESSOR_HYBI_HPP
#define WEBSOCKET_PROCESSOR_HYBI_HPP

#include "processor.hpp"
#include "hybi_header.hpp"

#include "../base64/base64.h"
#include "../sha1/sha1.h"

#include <boost/algorithm/string.hpp>

#ifdef IGNORE
	#undef IGNORE
#endif // #ifdef IGNORE

#ifdef min
	#undef min
#endif // #ifdef min

namespace websocketpp {
namespace processor {

namespace hybi_state {
    enum value {
        READ_HEADER = 0,
        READ_PAYLOAD = 1,
        READY = 2,
        IGNORE = 3
    };
}

/*

client case
end user asks connection for next message
- connection returns the next avaliable message or throws if none are ready
- connection resets the message and fills in a new masking key
end user calls set payload with const ref string to final payload
- set opcode... argument to set payload?
- set payload checks utf8 if copies from source, masks, and stores in payload.


prepare (hybi):
- writes header (writes opcode, payload length, masking key, fin bit, mask bit)


server case
end user asks connection for next message
- connection returns the next avaliable message or throws if none are ready
- connection resets the message and sets masking to off.
end user calls set payload with const ref string to final payload
- std::copy msg to payload

prepare
- writes header (writes opcode, payload length, fin bit, mask bit)


int reference_count
std::list< std::pair< std::string,std::string > >


*/

/*class hybi_message {
public:
    hybi_message(frame::opcode::value opcode) : m_processed(false) {
        
    }
    
    void reset() {
        
    }
private:
    bool        m_processed;
    std::string m_header;
    std::string m_payload;
};*/

// connection must provide:
// int32_t get_rng();
// message::data_ptr get_data_message();
// message::control_ptr get_control_message();
// bool is_secure();

template <class connection_type>
class hybi : public processor_base {
public:
    hybi(connection_type &connection) 
     : m_connection(connection),
       m_write_frame(connection)
    {
        reset();
    }       
    
    void validate_handshake(const http::parser::request& request) const {
        std::stringstream err;
        std::string h;
        
        if (request.method() != "GET") {
            err << "Websocket handshake has invalid method: " 
            << request.method();
            
            throw(http::exception(err.str(),http::status_code::BAD_REQUEST));
        }
        
        // TODO: allow versions greater than 1.1
        if (request.version() != "HTTP/1.1") {
            err << "Websocket handshake has invalid HTTP version: " 
            << request.method();
            
            throw(http::exception(err.str(),http::status_code::BAD_REQUEST));
        }
        
        // verify the presence of required headers
        if (request.header("Host") == "") {
            throw(http::exception("Required Host header is missing",http::status_code::BAD_REQUEST));
        }
        
        h = request.header("Upgrade");
        if (h == "") {
            throw(http::exception("Required Upgrade header is missing",http::status_code::BAD_REQUEST));
        } else if (!boost::ifind_first(h,"websocket")) {
            err << "Upgrade header \"" << h << "\", does not contain required token \"websocket\"";
            throw(http::exception(err.str(),http::status_code::BAD_REQUEST));
        }
        
        h = request.header("Connection");
        if (h == "") {
            throw(http::exception("Required Connection header is missing",http::status_code::BAD_REQUEST));
        } else if (!boost::ifind_first(h,"upgrade")) {
            err << "Connection header, \"" << h 
            << "\", does not contain required token \"upgrade\"";
            throw(http::exception(err.str(),http::status_code::BAD_REQUEST));
        }
        
        if (request.header("Sec-WebSocket-Key") == "") {
            throw(http::exception("Required Sec-WebSocket-Key header is missing",http::status_code::BAD_REQUEST));
        }
        
        h = request.header("Sec-WebSocket-Version");
        if (h == "") {
            throw(http::exception("Required Sec-WebSocket-Version header is missing",http::status_code::BAD_REQUEST));
        } else {
            int version = atoi(h.c_str());
            
            if (version != 7 && version != 8 && version != 13) {
                err << "This processor doesn't support WebSocket protocol version "
                << version;
                throw(http::exception(err.str(),http::status_code::BAD_REQUEST));
            }
        }
    }
    
    std::string get_origin(const http::parser::request& request) const {
        std::string h = request.header("Sec-WebSocket-Version");
        int version = atoi(h.c_str());
        
        if (version == 13) {
            return request.header("Origin");
        } else if (version == 7 || version == 8) {
            return request.header("Sec-WebSocket-Origin");
        } else {
            throw(http::exception("Could not determine origin header. Check Sec-WebSocket-Version header",http::status_code::BAD_REQUEST));
        }
    }
    
    uri_ptr get_uri(const http::parser::request& request) const {
        std::string h = request.header("Host");
        
        size_t last_colon = h.rfind(":");
        size_t last_sbrace = h.rfind("]");
                
        // no : = hostname with no port
        // last : before ] = ipv6 literal with no port
        // : with no ] = hostname with port
        // : after ] = ipv6 literal with port
        if (last_colon == std::string::npos || 
            (last_sbrace != std::string::npos && last_sbrace > last_colon))
        {
            return uri_ptr(new uri(m_connection.is_secure(),h,request.uri()));
        } else {
            return uri_ptr(new uri(m_connection.is_secure(),
                                   h.substr(0,last_colon),
                                   h.substr(last_colon+1),
                                   request.uri()));
        }
        
        // TODO: check if get_uri is a full uri
    }
    
    void handshake_response(const http::parser::request& request,
                            http::parser::response& response) 
    {
        std::string server_key = request.header("Sec-WebSocket-Key");
        server_key += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        
        SHA1        sha;
        uint32_t    message_digest[5];
        
        sha.Reset();
        sha << server_key.c_str();
        
        if (sha.Result(message_digest)){
            // convert sha1 hash bytes to network byte order because this sha1
            //  library works on ints rather than bytes
            for (int i = 0; i < 5; i++) {
                message_digest[i] = htonl(message_digest[i]);
            }
            
            server_key = base64_encode(
                                       reinterpret_cast<const unsigned char*>
                                       (message_digest),20
                                       );
            
            // set handshake accept headers
            response.replace_header("Sec-WebSocket-Accept",server_key);
            response.add_header("Upgrade","websocket");
            response.add_header("Connection","Upgrade");
        } else {
            //m_endpoint->elog().at(log::elevel::RERROR) 
            //<< "Error computing handshake sha1 hash" << log::endl;
            // TODO: make sure this error path works
            response.set_status(http::status_code::INTERNAL_SERVER_ERROR);
        }
    }
    
    void consume(std::istream& s) {
        while (s.good() && m_state != hybi_state::READY) {
            try {
                switch (m_state) {
                    case hybi_state::READ_HEADER:
                        process_header(s);
                        break;
                    case hybi_state::READ_PAYLOAD:
                        process_payload(s);
                        break;
                    case hybi_state::READY:
                        // shouldn't be here..
                        break;
                    case hybi_state::IGNORE:
                        s.ignore(m_payload_left);
                        m_payload_left -= static_cast<size_t>(s.gcount());
                        
                        if (m_payload_left == 0) {
                            reset();
                        }
                        
                        break;
                    default:
                        break;
                }
            } catch (const processor::exception& e) {
                if (e.code() != processor::error::OUT_OF_MESSAGES) {
                    // The out of messages exception acts as an inturrupt rather
                    // than an error. In that case we don't want to reset 
                    // processor state. In all other cases we are aborting 
                    // processing of the message in flight and want to reset the
                    // processor for a new message.
                    if (m_header.ready()) {
                        m_header.reset();
                        ignore();
                    }
                }
                
                throw e;
            }
        }
    }
    
    // Sends the processor an inturrupt signal instructing it to ignore the next
    // num bytes and then reset itself. This is used to flush a bad frame out of
    // the read buffer.
    void ignore() {
        m_state = hybi_state::IGNORE;
    }
    
    void process_header(std::istream& s) {
        m_header.consume(s);
        
        if (m_header.ready()) {
            // Get a free message from the read queue for the type of the 
            // current message
            if (m_header.is_control()) {
                process_control_header();
            } else {
                process_data_header();
            }
        }
    }
    
    void process_control_header() {
        m_control_message = m_connection.get_control_message();
        
        if (!m_control_message) {
            throw processor::exception("Out of control messages",
                                       processor::error::OUT_OF_MESSAGES);
        }
        
        m_control_message->reset(m_header.get_opcode(),m_header.get_masking_key());
        
        m_payload_left = static_cast<size_t>(m_header.get_payload_size());
        
        if (m_payload_left == 0) {
            process_frame();
        } else {
            m_state = hybi_state::READ_PAYLOAD;
        }
    }
    
    void process_data_header() {
        if (!m_data_message) {
            // This is a new message. No continuation frames allowed.
            if (m_header.get_opcode() == frame::opcode::CONTINUATION) {
                throw processor::exception("Received continuation frame without an outstanding message.",processor::error::PROTOCOL_VIOLATION);
            }
            
            m_data_message = m_connection.get_data_message();
            
            if (!m_data_message) {
                throw processor::exception("Out of data messages",
                                           processor::error::OUT_OF_MESSAGES);
            }
            
            m_data_message->reset(m_header.get_opcode());
        } else {
            // A message has already been started. Continuation frames only!
            if (m_header.get_opcode() != frame::opcode::CONTINUATION) {
                throw processor::exception("Received new message before the completion of the existing one.",processor::error::PROTOCOL_VIOLATION);
            }
        }
        
        m_payload_left = static_cast<size_t>(m_header.get_payload_size());
        
        if (m_payload_left == 0) {
            process_frame();
        } else {
            // each frame has a new masking key
            m_data_message->set_masking_key(m_header.get_masking_key());
            m_state = hybi_state::READ_PAYLOAD;
        }
    }
    
    void process_payload(std::istream& input) {
        //std::cout << "payload left 1: " << m_payload_left << std::endl;
        size_t num;
        
        // read bytes into processor buffer. Read the lesser of the buffer size
        // and the number of bytes left in the payload.
        
        input.read(m_payload_buffer, std::min(m_payload_left, PAYLOAD_BUFFER_SIZE));
        num = static_cast<size_t>(input.gcount());
                
        if (input.bad()) {
            throw processor::exception("istream readsome error",
                                       processor::error::FATAL_ERROR);
        }
        
        if (num == 0) {
            return;
        }
        
        m_payload_left -= num;
        
        // tell the appropriate message to process the bytes.
        if (m_header.is_control()) {
            m_control_message->process_payload(m_payload_buffer,num);
        } else {
            //m_connection.alog().at(log::alevel::DEVEL) << "process_payload. Size:  " << m_payload_left << log::endl;
            m_data_message->process_payload(m_payload_buffer,num);
        }
                
        if (m_payload_left == 0) {
            process_frame();
        }
        
    }
    
    void process_frame() {
        if (m_header.get_fin()) {
            if (m_header.is_control()) {
                m_control_message->complete();
            } else {
                m_data_message->complete();
            }
            m_state = hybi_state::READY;
        } else {
            reset();
        }
    }
    
    bool ready() const {
        return m_state == hybi_state::READY;
    }
    
    bool is_control() const {
        return m_header.is_control();
    }
    
    // note this can only be called once
    message::data_ptr get_data_message() {
        message::data_ptr p = m_data_message;
        m_data_message.reset();
        return p;
    }
    
    // note this can only be called once
    message::control_ptr get_control_message() {
        message::control_ptr p = m_control_message;
        m_control_message.reset();
        return p;
    }
    
    void reset() {
        m_state = hybi_state::READ_HEADER;
        m_header.reset();
    }
    
    uint64_t get_bytes_needed() const {
        switch (m_state) {
            case hybi_state::READ_HEADER:
                return m_header.get_bytes_needed();
            case hybi_state::READ_PAYLOAD:
            case hybi_state::IGNORE:
                return m_payload_left;
            case hybi_state::READY:
                return 0;
            default:
                throw "shouldn't be here";
        }
    }
    
    // TODO: replace all this to remove all lingering dependencies on 
    // websocket_frame
    binary_string_ptr prepare_frame(frame::opcode::value opcode,
                                    bool mask,
                                    const utf8_string& payload) {
        /*if (opcode != frame::opcode::TEXT) {
         // TODO: hybi_legacy doesn't allow non-text frames.
         throw;
         }*/
        
        // TODO: utf8 validation on payload.
        
        
        
        binary_string_ptr response(new binary_string(0));
        
        
        m_write_frame.reset();
        m_write_frame.set_opcode(opcode);
        m_write_frame.set_masked(mask);
        
        m_write_frame.set_fin(true);
        m_write_frame.set_payload(payload);
        
        
        m_write_frame.process_payload();
        
        // TODO
        response->resize(m_write_frame.get_header_len()+m_write_frame.get_payload().size());
        
        // copy header
        std::copy(m_write_frame.get_header(),m_write_frame.get_header()+m_write_frame.get_header_len(),response->begin());
        
        // copy payload
        std::copy(m_write_frame.get_payload().begin(),m_write_frame.get_payload().end(),response->begin()+m_write_frame.get_header_len());
        
        
        return response;
    }
    
    binary_string_ptr prepare_frame(frame::opcode::value opcode,
                                    bool mask,
                                    const binary_string& payload) {
        /*if (opcode != frame::opcode::TEXT) {
         // TODO: hybi_legacy doesn't allow non-text frames.
         throw;
         }*/
        
        // TODO: utf8 validation on payload.
        
        binary_string_ptr response(new binary_string(0));
        
        m_write_frame.reset();
        m_write_frame.set_opcode(opcode);
        m_write_frame.set_masked(mask);
        m_write_frame.set_fin(true);
        m_write_frame.set_payload(payload);
        
        m_write_frame.process_payload();
        
        // TODO
        response->resize(m_write_frame.get_header_len()+m_write_frame.get_payload().size());
        
        // copy header
        std::copy(m_write_frame.get_header(),m_write_frame.get_header()+m_write_frame.get_header_len(),response->begin());
        
        // copy payload
        std::copy(m_write_frame.get_payload().begin(),m_write_frame.get_payload().end(),response->begin()+m_write_frame.get_header_len());
        
        return response;
    }
    
    /*binary_string_ptr prepare_close_frame(close::status::value code,
                                          bool mask,
                                          const std::string& reason) {
        binary_string_ptr response(new binary_string(0));
        
        m_write_frame.reset();
        m_write_frame.set_opcode(frame::opcode::CLOSE);
        m_write_frame.set_masked(mask);
        m_write_frame.set_fin(true);
        m_write_frame.set_status(code,reason);
        
        m_write_frame.process_payload();
        
        // TODO
        response->resize(m_write_frame.get_header_len()+m_write_frame.get_payload().size());
        
        // copy header
        std::copy(m_write_frame.get_header(),m_write_frame.get_header()+m_write_frame.get_header_len(),response->begin());
        
        // copy payload
        std::copy(m_write_frame.get_payload().begin(),m_write_frame.get_payload().end(),response->begin()+m_write_frame.get_header_len());
        
        return response;
    }*/
    
    // new prepare frame stuff
    void prepare_frame(message::data_ptr msg) {
        assert(msg);
        if (msg->get_prepared()) {
            return;
        }
        
        msg->validate_payload();
        
        bool masked = !m_connection.is_server();
        int32_t key = m_connection.rand();
        
        m_write_header.reset();
        m_write_header.set_fin(true);
        m_write_header.set_opcode(msg->get_opcode());
        m_write_header.set_masked(masked,key);
        m_write_header.set_payload_size(msg->get_payload().size());
        m_write_header.complete();
        
        msg->set_header(m_write_header.get_header_bytes());
        
        if (masked) {
            msg->set_masking_key(key);
            msg->mask();
        }
        
        msg->set_prepared(true);
    }
    
    void prepare_close_frame(message::data_ptr msg, 
                             close::status::value code,
                             const std::string& reason)
    {
        assert(msg);
        if (msg->get_prepared()) {
            return;
        }
        
        // set close payload
        if (code != close::status::NO_STATUS) {
            const uint16_t payload = htons(static_cast<u_short>(code));
        
            msg->set_payload(std::string(reinterpret_cast<const char*>(&payload), 2));
            msg->append_payload(reason);
        }
        
        // prepare rest of frame
        prepare_frame(msg);
    }
private:
    // must be divisible by 8 (some things are hardcoded for 4 and 8 byte word
    // sizes
    static const size_t     PAYLOAD_BUFFER_SIZE = 512;
    
    connection_type&        m_connection;
    int                     m_state;
    
    message::data_ptr       m_data_message;
    message::control_ptr    m_control_message;
    hybi_header             m_header;
    hybi_header             m_write_header;
    size_t                  m_payload_left;
    
    char                    m_payload_buffer[PAYLOAD_BUFFER_SIZE];
    
    frame::parser<connection_type>  m_write_frame; // TODO: refactor this out
};  

template <class connection_type>
const size_t hybi<connection_type>::PAYLOAD_BUFFER_SIZE;

} // namespace processor
} // namespace websocketpp

#endif // WEBSOCKET_PROCESSOR_HYBI_HPP
