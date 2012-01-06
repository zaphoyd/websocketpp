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

#ifndef WEBSOCKETPP_ENDPOINT_HPP
#define WEBSOCKETPP_ENDPOINT_HPP

#include "connection.hpp"
#include "sockets/plain.hpp" // should this be here?
#include "logger/logger.hpp"

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>

#include <iostream>
#include <set>

namespace websocketpp {
    
/// endpoint_base provides core functionality that needs to be constructed
/// before endpoint policy classes are constructed.
class endpoint_base {
protected:
    boost::asio::io_service m_io_service;
};

/// Describes a configurable WebSocket endpoint.
/**
 * The endpoint class template provides a configurable WebSocket endpoint 
 * capable that manages WebSocket connection lifecycles. endpoint is a host 
 * class to a series of enriched policy classes that together provide the public
 * interface for a specific type of WebSocket endpoint.
 * 
 * @par Thread Safety
 * @e Distinct @e objects: Safe.@n
 * @e Shared @e objects: Will be safe when complete.
 */
template <
    template <class> class role,
    template <class> class socket = socket::plain,
    template <class> class logger = log::logger>
class endpoint 
 : public endpoint_base,
   public role< endpoint<role,socket> >,
   public socket< endpoint<role,socket> > 
{
public:
    /// Type of the traits class that stores endpoint related types.
    typedef endpoint_traits< endpoint<role,socket,logger> > traits;
    
    /// The type of the endpoint itself.
    typedef typename traits::type type;
    /// The type of the role policy.
    typedef typename traits::role_type role_type;
    /// The type of the socket policy.
    typedef typename traits::socket_type socket_type;
    /// The type of the access logger based on the logger policy.
    typedef typename traits::alogger_type alogger_type;
    /// The type of the error logger based on the logger policy.
    typedef typename traits::elogger_type elogger_type;
    /// The type of the connection that this endpoint creates.
    typedef typename traits::connection_type connection_type;
    /// A shared pointer to the type of connection that this endpoint creates.
    typedef typename traits::connection_ptr connection_ptr;
    /// Interface (ABC) that handlers for this type of endpoint must impliment
    /// role policy and socket policy both may add methods to this interface
    typedef typename traits::handler handler;
    /// A shared pointer to the base class that all handlers for this endpoint
    /// must derive from.
    typedef typename traits::handler_ptr handler_ptr;
    
    // Friend is used here to allow the CRTP base classes to access member 
    // functions in the derived endpoint. This is done to limit the use of 
    // public methods in endpoint and its CRTP bases to only those methods 
    // intended for end-application use.
    friend class role< endpoint<role,socket> >;
    friend class socket< endpoint<role,socket> >;
    friend class connection<type,role< type >::template connection,socket< type >::template connection>;
    
    // Highly simplified and preferred C++11 version:
    // friend role_type; 
    // friend socket_type;
    // friend connection_type;
    
    /// Construct an endpoint.
    /**
     * This constructor creates an endpoint and registers the default connection
     * handler.
     * 
     * @param handler A shared_ptr to the handler to use as the default handler
     * when creating new connections.
     */
    explicit endpoint(handler_ptr handler) 
     : role_type(m_io_service),
       socket_type(m_io_service),
       m_handler(handler) {}
    
    /// Returns a reference to the endpoint's access logger.
    /**
     * @returns A reference to the endpoint's access logger. See @ref logger
     * for more details about WebSocket++ logging policy classes.
     *
     * @par Example
     * To print a message to the access log of endpoint e at access level DEVEL:
     * @code
     * e.alog().at(log::alevel::DEVEL) << "message" << log::endl;
     * @endcode
     */
    alogger_type& alog() {
        return m_alog;
    }
    
    /// Returns a reference to the endpoint's error logger.
    /**
     * @returns A reference to the endpoint's error logger. See @ref logger
     * for more details about WebSocket++ logging policy classes.
     *
     * @par Example
     * To print a message to the error log of endpoint e at access level DEVEL:
     * @code
     * e.elog().at(log::elevel::DEVEL) << "message" << log::endl;
     * @endcode
     */
    elogger_type& elog() {
        return m_elog;
    }
    
    /// Updates the default handler to be used for future connections
    /**
     * @param new_handler A shared pointer to the new default handler. Must not
     * be NULL.
     */
    void set_handler(handler_ptr new_handler) {
        if (!new_handler) {
            elog().at(log::elevel::FATAL) 
                << "Tried to switch to a NULL handler." << log::endl;
            throw "TODO: handlers can't be null";
        }
        
        m_handler = new_handler;
    }
protected:
    /// Creates and returns a new connection
    /**
     * This function creates a new connection of the type and passes it a 
     * reference to this as well as a shared pointer to the default connection
     * handler. The newly created connection is added to the endpoint's 
     * management list. The endpoint will retain this pointer until 
     * remove_connection is called to remove it.
     * 
     * @returns A shared pointer to the newly created connection.
     */
    connection_ptr create_connection() {
        connection_ptr new_connection(new connection_type(*this,get_handler()));
        m_connections.insert(new_connection);
        
        alog().at(log::alevel::DEVEL) << "Connection created: count is now: " << m_connections.size() << log::endl;
        
        return new_connection;
    }
    
    /// Removes a connection from the list managed by this endpoint.
    /**
     * This function erases a connection from the list managed by the endpoint.
     * After this function returns, endpoint all async events related to this
     * connection should be canceled and neither ASIO nor this endpoint should
     * have a pointer to this connection. Unless the end user retains a copy of
     * the shared pointer the connection will be freed and any state it 
     * contained (close code status, etc) will be lost.
     * 
     * @param con A shared pointer to a connection created by this endpoint.
     */
    void remove_connection(connection_ptr con) {
        m_connections.erase(con);
        
        alog().at(log::alevel::DEVEL) << "Connection removed: count is now: " << m_connections.size() << log::endl;
    }
    
    /// Gets a shared pointer to this endpoint's default connection handler
    handler_ptr get_handler() {
        return m_handler;
    }
    
    message::data_ptr get_data_message() {
		// if we have one of this type free
		if (!m_read_queue_avaliable.empty()) {
			message::data_ptr p = m_read_queue_avaliable.front();
			m_read_queue_avaliable.pop();
			m_read_queue_used.insert(p);
			return p;
		} else {
            if (m_read_queue_used.size() > 10) {
                return message::data_ptr();
            } else {
                m_read_queue_avaliable.push(message::data_ptr(new message::data()));
                alog().at(log::alevel::DEVEL) 
                << "Allocating new data message. Count is now: " 
                << m_read_queue_used.size() + 1 
                << log::endl;
                return get_data_message();
            }
		}
	}
    
    void recycle(message::data_ptr p) {
		if (m_read_queue_used.erase(p) == 0) {
			// tried to recycle a pointer we don't control.
		} else {
			m_read_queue_avaliable.push(p);
			
            // wake next
            if (!m_read_waiting.empty()) {
                connection_ptr next = m_read_waiting.front();
            
                /*endpoint_base::m_io_service.post(
					boost::bind(
					    &connection_type::handle_read_frame,
					    next,
					    boost::system::error_code()
				    )
			    );*/
                (*next).handle_read_frame(boost::system::error_code());
                
                m_read_waiting.pop();
            }
            // wake all
            /*std::list<connection_ptr>::iterator it;
            
            for (it = m_read_waiting.begin(); it != m_read_waiting.end(); it++) {
                endpoint_base::m_io_service.post(
					boost::bind(
					    &connection_type::handle_read_frame,
					    *it,
					    boost::system::error_code()
				    )
			    ); 
            }
            
            m_read_waiting.empty();*/
		}
	}
    
    void wait(connection_ptr con) {
        m_read_waiting.push(con);
    }
    
private:
    handler_ptr                 m_handler;
    std::set<connection_ptr>    m_connections;
    alogger_type                m_alog;
    elogger_type                m_elog;
    
    // mssage buffers
    std::queue<message::data_ptr>	m_read_queue_avaliable;
	std::set<message::data_ptr>		m_read_queue_used;
    std::queue<connection_ptr>      m_read_waiting;
};

/// traits class that allows looking up relevant endpoint types by the fully 
/// defined endpoint type.
template <
    template <class> class role,
    template <class> class socket,
    template <class> class logger>
struct endpoint_traits< endpoint<role, socket, logger> > {
    /// The type of the endpoint itself.
    typedef endpoint<role,socket,logger> type;
    
    /// The type of the role policy.
    typedef role< type > role_type;
    /// The type of the socket policy.
    typedef socket< type > socket_type;
    
    /// The type of the access logger based on the logger policy.
    typedef logger<log::alevel::value> alogger_type;
    /// The type of the error logger based on the logger policy.
    typedef logger<log::elevel::value> elogger_type;
    
    /// The type of the connection that this endpoint creates.
    typedef connection<type,
                       role< type >::template connection,
                       socket< type >::template connection> connection_type;
    /// A shared pointer to the type of connection that this endpoint creates.
    typedef boost::shared_ptr<connection_type> connection_ptr;
    
    class handler;
    
    /// A shared pointer to the base class that all handlers for this endpoint
    /// must derive from.
    typedef boost::shared_ptr<handler> handler_ptr;
    
    /// Interface (ABC) that handlers for this type of endpoint may impliment.
    /// role policy and socket policy both may add methods to this interface
    class handler : public role_type::handler_interface,
                    public socket_type::handler_interface
    {
    public:
        /// on_load is the first callback called for a handler after a new
        /// connection has been transferred to it mid flight.
        /**
         * @param connection A shared pointer to the connection that was transferred
         * @param old_handler A shared pointer to the previous handler
         */
        virtual void on_load(connection_ptr connection, handler_ptr old_handler) {}
        /// on_unload is the last callback called for a handler before control
        /// of a connection is handed over to a new handler mid flight.
        /**
         * @param connection A shared pointer to the connection being transferred
         * @param old_handler A shared pointer to the new handler
         */
        virtual void on_unload(connection_ptr connection, handler_ptr new_handler) {}
    };
    
};

} // namespace websocketpp

#endif // WEBSOCKETPP_ENDPOINT_HPP
