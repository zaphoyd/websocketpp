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

#ifndef WEBSOCKETPP_MESSAGE_BUFFER_FIXED_HPP
#define WEBSOCKETPP_MESSAGE_BUFFER_FIXED_HPP

#include <websocketpp/message_buffer/fixed_message.hpp>
#include <websocketpp/common/memory.hpp>
#include <websocketpp/frame.hpp>

namespace websocketpp {
namespace message_buffer {
namespace fixed {

class con_msg_manager;
class endpoint_msg_manager;

struct policy {
    typedef websocketpp::message_buffer::fixed_message message;
    typedef websocketpp::message_buffer::fixed::con_msg_manager con_msg_manager;
    typedef websocketpp::message_buffer::fixed::endpoint_msg_manager endpoint_msg_manager;
};

/// A connection message manager that allocates a new message for each
/// request.
class con_msg_manager {
public:
    typedef con_msg_manager type;

    typedef std::shared_ptr<type> ptr;

    typedef policy::message message;
    typedef policy::message::ptr message_ptr;

    con_msg_manager()
      : m_incoming_message(new message())
      , m_incoming_message_busy(false)
      , m_outgoing_message(new message())
      , m_outgoing_message_busy(false) {}

    /// Get an empty message buffer
    /**
     * @return A shared pointer to an empty new message
     */
    message_ptr get_message() {
        return message_ptr(new message());
    }

    message_ptr get_incoming_message(frame::opcode::value op, size_t size) {
        if (frame::opcode::is_control(op)) {
            return message_ptr(new message(op, size));
        } else {
            if (m_incoming_message_busy) {
                return message_ptr();
            } else {
                m_incoming_message->set_opcode(op);
                m_incoming_message->reserve(size);
                return m_incoming_message;
            }
        }
    }

    message_ptr get_outgoing_message(frame::opcode::value op, size_t size) {
        if (frame::opcode::is_control(op)) {
            return message_ptr(new message(op, size));
        } else {
            if (m_outgoing_message_busy) {
                return message_ptr();
            } else {
                m_outgoing_message->set_opcode(op);
                m_outgoing_message->reserve(size);
                return m_outgoing_message;
            }
        }
    }

    void message_handler_hook(message_ptr msg) {
        recycle(msg);
    }

    bool recycle(message_ptr msg) {
        if (msg == m_incoming_message) {
            m_incoming_message_busy = false;
            return true;
        } else if (msg == m_outgoing_message) {
            m_outgoing_message_busy = false;
            return true;
        } else {
            // not a message we are managing, ignore
            return false;
        }
    }
private:
    message_ptr m_incoming_message;
    bool m_incoming_message_busy;
    message_ptr m_outgoing_message;
    bool m_outgoing_message_busy;
};

/// An endpoint message manager that allocates a new manager for each
/// connection.
class endpoint_msg_manager {
public:
    typedef policy::con_msg_manager::ptr con_msg_man_ptr;

    /// Get a pointer to a connection message manager
    /**
     * @return A pointer to the requested connection message manager.
     */
    con_msg_man_ptr get_manager() const {
        return con_msg_man_ptr(new con_msg_manager());
    }
};

} // namespace fixed
} // namespace message_buffer
} // namespace websocketpp

#endif // WEBSOCKETPP_MESSAGE_BUFFER_FIXED_HPP
