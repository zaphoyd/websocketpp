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

#ifndef WEBSOCKETPP_ENDPOINT_IMPL_HPP
#define WEBSOCKETPP_ENDPOINT_IMPL_HPP

namespace websocketpp {

template <typename connection, typename config>
typename endpoint<connection,config>::connection_ptr
endpoint<connection,config>::create_connection() {
    m_alog.write(log::alevel::devel,"create_connection");
    //scoped_lock_type lock(m_state_lock);
    
    /*if (m_state == STOPPING || m_state == STOPPED) {
        return connection_ptr();
    }*/
    
    // Create a connection on the heap and manage it using a shared pointer
    connection_ptr con(new connection_type(m_is_server,m_user_agent,m_alog,
        m_elog, m_rng));
    
    connection_weak_ptr w(con);
    
    // Create a weak pointer on the heap using that shared_ptr.
    // Cast that weak pointer to void* and manage it using another shared_ptr
    // connection_hdl hdl(reinterpret_cast<void*>(new connection_weak_ptr(con)));

    // con->set_handle(hdl);

    //
    con->set_handle(w);
    
    // Copy default handlers from the endpoint
    con->set_open_handler(m_open_handler);
    con->set_close_handler(m_close_handler);
    con->set_fail_handler(m_fail_handler);
    con->set_ping_handler(m_ping_handler);
    con->set_pong_handler(m_pong_handler);
    con->set_pong_timeout_handler(m_pong_timeout_handler);
    con->set_interrupt_handler(m_interrupt_handler);
    con->set_http_handler(m_http_handler);
    con->set_validate_handler(m_validate_handler);
    con->set_message_handler(m_message_handler);
    
    con->set_termination_handler(
        lib::bind(
            &type::remove_connection,
            this,
            lib::placeholders::_1
        )
    );
    
    lib::error_code ec;
    
    ec = transport_type::init(con);
    if (ec) {
        m_elog.write(log::elevel::fatal,ec.message());
        return connection_ptr();
    }
    
    scoped_lock_type lock(m_mutex);
    m_connections.insert(con);
    
    return con;
}

template <typename connection, typename config>
void endpoint<connection,config>::interrupt(connection_hdl hdl, 
    lib::error_code & ec)
{
    connection_ptr con = get_con_from_hdl(hdl,ec);
    if (ec) {return;}
    
    m_alog.write(log::alevel::devel,"Interrupting connection"+con.get());
    
    ec = con->interrupt();
}

template <typename connection, typename config>
void endpoint<connection,config>::interrupt(connection_hdl hdl) {
    lib::error_code ec;
    interrupt(hdl,ec);
    if (ec) { throw ec; }
}

template <typename connection, typename config>
void endpoint<connection,config>::send(connection_hdl hdl, 
    const std::string& payload, frame::opcode::value op, lib::error_code & ec)
{
    connection_ptr con = get_con_from_hdl(hdl,ec);
    if (ec) {return;}

    ec = con->send(payload,op);
}

template <typename connection, typename config>
void endpoint<connection,config>::send(connection_hdl hdl, const std::string& 
    payload, frame::opcode::value op)
{
    lib::error_code ec;
    send(hdl,payload,op,ec);
    if (ec) { throw ec; }
}

template <typename connection, typename config>
void endpoint<connection,config>::send(connection_hdl hdl, const void * payload,
    size_t len, frame::opcode::value op, lib::error_code & ec)
{
    connection_ptr con = get_con_from_hdl(hdl,ec);
    if (ec) {return;}
    ec = con->send(payload,len,op);
}

template <typename connection, typename config>
void endpoint<connection,config>::send(connection_hdl hdl, const void * payload,
    size_t len, frame::opcode::value op)
{
    lib::error_code ec;
    send(hdl,payload,len,op,ec);
    if (ec) { throw ec; }
}

template <typename connection, typename config>
void endpoint<connection,config>::send(connection_hdl hdl, message_ptr msg, 
    lib::error_code & ec)
{
    connection_ptr con = get_con_from_hdl(hdl,ec);
    if (ec) {return;}
    ec = con->send(msg);
}

template <typename connection, typename config>
void endpoint<connection,config>::send(connection_hdl hdl, message_ptr msg) {
    lib::error_code ec;
    send(hdl,msg,ec);
    if (ec) { throw ec; }
}

template <typename connection, typename config>
void endpoint<connection,config>::close(connection_hdl hdl, 
    const close::status::value code, const std::string & reason, 
    lib::error_code & ec)
{
    connection_ptr con = get_con_from_hdl(hdl,ec);
    if (ec) {return;}
    con->close(code,reason,ec);
}
    
template <typename connection, typename config>
void endpoint<connection,config>::close(connection_hdl hdl, 
    const close::status::value code, const std::string & reason)
{
    lib::error_code ec;
    close(hdl,code,reason,ec);
    if (ec) { throw ec; }
}

template <typename connection, typename config>
void endpoint<connection,config>::remove_connection(connection_ptr con) {
    std::stringstream s;
    s << "remove_connection. New count: " << m_connections.size()-1;
    m_alog.write(log::alevel::devel,s.str());
    
    scoped_lock_type lock(m_mutex);
        
    // unregister the termination handler
    con->set_termination_handler(termination_handler());
    
    m_connections.erase(con);
}

} // namespace websocketpp

#endif // WEBSOCKETPP_ENDPOINT_IMPL_HPP
