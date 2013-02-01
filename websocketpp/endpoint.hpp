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

#ifndef WEBSOCKETPP_ENDPOINT_HPP
#define WEBSOCKETPP_ENDPOINT_HPP

#include <websocketpp/connection.hpp>

#include <iostream>
#include <set>

namespace websocketpp {

static const char user_agent[] = "WebSocket++/0.3.0dev";


// creates and manages connections
template <typename connection, typename config>
class endpoint : public config::transport_type {
public:
	// Import appropriate types from our helper class
	// See endpoint_types for more details.
	typedef endpoint<connection,config> type;
	
    /// Type of the transport component of this endpoint
    typedef typename config::transport_type transport_type;
    /// Type of the concurrency component of this endpoint
	typedef typename config::concurrency_type concurrency_type;

    /// Type of the connections that this endpoint creates
	typedef connection connection_type;
    /// Shared pointer to connection_type
	typedef typename connection_type::ptr connection_ptr;
    /// Weak pointer to connection type
	typedef typename connection_type::weak_ptr connection_weak_ptr;
    
    /// Type of the transport component of the connections that this endpoint 
    /// creates
    typedef typename transport_type::transport_con_type transport_con_type;
    /// Type of a shared pointer to the transport component of the connections
    /// that this endpoint creates.
    typedef typename transport_con_type::ptr transport_con_ptr;
	
    /// Type of message_handler
    typedef typename connection_type::message_handler message_handler;
    /// Type of message pointers that this endpoint uses
    typedef typename connection_type::message_ptr message_ptr;
    
    /// Type of error logger
    typedef typename config::elog_type elog_type;
    /// Type of access logger
	typedef typename config::alog_type alog_type;
    
    // TODO: organize these
	typedef typename connection_type::termination_handler termination_handler;
	
    typedef lib::shared_ptr<connection_weak_ptr> hdl_type;

	explicit endpoint(bool is_server)
 	  : m_elog(&std::cerr)
 	  , m_user_agent(::websocketpp::user_agent)
 	  , m_is_server(is_server)
	{
		m_alog.set_channels(0xffffffff);
		m_elog.set_channels(0x00000000);
		
		m_alog.write(log::alevel::devel,"endpoint constructor");

        transport_type::init_logging(&m_elog,&m_alog);
	}
	
	/// Returns the user agent string that this endpoint will use
	/**
	 * Returns the user agent string that this endpoint will use when creating
	 * new connections. 
	 *
	 * The default value for this version is WebSocket++/0.3.0dev
	 *
	 * @return The user agent string.
	 */ 
	std::string get_user_agent() const {
        scoped_lock_type guard(m_mutex);
		return m_user_agent;
	}
	
	/// Sets the user agent string that this endpoint will use
	/**
	 * Sets the user agent string that this endpoint will use when creating
	 * new connections. Changing this value will only affect future connections.
	 *
	 * For best results set this before accepting or opening connections.
	 *
	 * The default value for this version is WebSocket++/0.3.0dev
	 *
	 * @param ua The string to set the user agent to.
	 */ 
	void set_user_agent(const std::string& ua) {
        scoped_lock_type guard(m_mutex);
		m_user_agent = ua;
	}
	
    /// Returns whether or not this endpoint is a server.
    /**
     * @return Whether or not this endpoint is a server
     */
	bool is_server() const {
        return m_is_server;
    }
    
    /********************************/
    /* Pass-through logging adaptor */
    /********************************/
    
    /// Set Access logging channel
    /**
     * Set the access logger's channel value. The value is a number whose
     * interpretation depends on the logging policy in use.
     * 
     * @param channels The channel value(s) to set
     */
    void set_access_channels(log::level channels) {
        m_alog.set_channels(channels);
    }
    
    /// Clear Access logging channels
    /**
     * Clear the access logger's channel value. The value is a number whose
     * interpretation depends on the logging policy in use.
     * 
     * @param channels The channel value(s) to clear
     */
    void clear_access_channels(log::level channels) {
        m_alog.clear_channels(channels);
    }
    
    /// Set Error logging channel
    /**
     * Set the error logger's channel value. The value is a number whose
     * interpretation depends on the logging policy in use.
     * 
     * @param channels The channel value(s) to set
     */
    void set_error_channels(log::level channels) {
        m_elog.set_channels(channels);
    }
    
    /// Clear Error logging channels
    /**
     * Clear the error logger's channel value. The value is a number whose
     * interpretation depends on the logging policy in use.
     * 
     * @param channels The channel value(s) to clear
     */
    void clear_error_channels(log::level channels) {
        m_elog.clear_channels(channels);
    }
    
    /// Get reference to access logger
    /**
     * TODO
     * 
     * @return A reference to the access logger
     */
    alog_type& get_alog() {
        return m_alog;
    }
    
    /// Get reference to error logger
    /**
     * TODO
     * 
     * @return A reference to the error logger
     */
    elog_type& get_elog() {
        return m_elog;
    }
    
    /*************************/
    /* Set Handler functions */
    /*************************/

    void set_open_handler(open_handler h) {
        m_alog.write(log::alevel::devel,"set_open_handler");
        scoped_lock_type guard(m_mutex);
        m_open_handler = h;
    }
    void set_close_handler(close_handler h) {
        m_alog.write(log::alevel::devel,"set_close_handler");
        scoped_lock_type guard(m_mutex);
        m_close_handler = h;
    }
    void set_fail_handler(fail_handler h) {
        scoped_lock_type guard(m_mutex);
        m_fail_handler = h;
    }
    void set_ping_handler(ping_handler h) {
        scoped_lock_type guard(m_mutex);
        m_ping_handler = h;
    }
    void set_pong_handler(pong_handler h) {
        scoped_lock_type guard(m_mutex);
        m_pong_handler = h;
    }
    void set_pong_timeout_handler(pong_timeout_handler h) {
        scoped_lock_type guard(m_mutex);
        m_pong_timeout_handler = h;
    }
    void set_interrupt_handler(interrupt_handler h) {
        scoped_lock_type guard(m_mutex);
        m_interrupt_handler = h;
    }
    void set_http_handler(http_handler h) {
        scoped_lock_type guard(m_mutex);
        m_http_handler = h;
    }
    void set_validate_handler(validate_handler h) {
        scoped_lock_type guard(m_mutex);
        m_validate_handler = h;
    }
    void set_message_handler(message_handler h) {
        m_alog.write(log::alevel::devel,"set_message_handler");
        scoped_lock_type guard(m_mutex);
        m_message_handler = h;
    }
    
    /*************************************/
    /* Connection pass through functions */
    /*************************************/
    
    /**
     * Is thread safe if transport is
     */
    void interrupt(connection_hdl hdl, lib::error_code & ec) {
        connection_ptr con = get_con_from_hdl(hdl);
        if (!con) {
            ec = error::make_error_code(error::bad_connection);
            return;
        }
        
        m_alog.write(log::alevel::devel,
            "Interrupting connection"+con.get());
        
        ec = con->interrupt();
    }
    
    /**
     * Is thread safe if transport is
     */
    void interrupt(connection_hdl hdl) {
        lib::error_code ec;
        interrupt(hdl,ec);
        if (ec) { throw ec; }
    }

    void send(connection_hdl hdl, const std::string& payload, 
        frame::opcode::value op, lib::error_code & ec)
    {
        connection_ptr con = get_con_from_hdl(hdl);
        if (!con) {
            ec = error::make_error_code(error::bad_connection);
            return;
        }

        ec = con->send(payload,op);
    }

    void send(connection_hdl hdl, const std::string& payload,
        frame::opcode::value op)
    {
        lib::error_code ec;
        send(hdl,payload,op,ec);
        if (ec) { throw ec; }
    }

    void send(connection_hdl hdl, message_ptr msg, lib::error_code & ec) {
        connection_ptr con = get_con_from_hdl(hdl);
        if (!con) {
            ec = error::make_error_code(error::bad_connection);
            return;
        }

        ec = con->send(msg);
    }

    void send(connection_hdl hdl, message_ptr msg) {
        lib::error_code ec;
        send(hdl,msg,ec);
        if (ec) { throw ec; }
    }
protected:
	// Import appropriate internal types from our policy classes
	typedef typename concurrency_type::scoped_lock_type scoped_lock_type;
	typedef typename concurrency_type::mutex_type mutex_type;
	
	connection_ptr create_connection();
	void remove_connection(connection_ptr con);
    
    /// Retrieves a connection_ptr from a connection_hdl
    /**
     * Converting a weak pointer to shared_ptr is not thread safe because the
     * pointer could be deleted at any time.
     *
     * @param hdl The connection handle to translate
     *
     * @return the connection_ptr. May be NULL if the handle was invalid.
     */
    connection_ptr get_con_from_hdl(connection_hdl hdl) {
        scoped_lock_type lock(m_mutex);
        connection_ptr con = lib::static_pointer_cast<connection_type,void>(
            hdl.lock());
        return con;
    }
    
    alog_type m_alog;
    elog_type m_elog;
private:
	// dynamic settings
	std::string					m_user_agent;
	
    open_handler                m_open_handler;
    close_handler               m_close_handler;
    fail_handler                m_fail_handler;
    ping_handler                m_ping_handler;
    pong_handler                m_pong_handler;
    pong_timeout_handler        m_pong_timeout_handler;
    interrupt_handler           m_interrupt_handler;
    http_handler                m_http_handler;
    validate_handler            m_validate_handler;
    message_handler             m_message_handler;
    
    
    
	// endpoint resources
	std::set<connection_ptr>	m_connections;
	
	// static settings
	const bool					m_is_server;
	
	// endpoint state
	mutex_type                  m_mutex;
};

} // namespace websocketpp

#include <websocketpp/impl/endpoint_impl.hpp>

#endif // WEBSOCKETPP_ENDPOINT_HPP
