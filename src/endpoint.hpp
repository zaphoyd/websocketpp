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
#include <boost/thread/recursive_mutex.hpp>
#include <boost/utility.hpp>

#include <iostream>
#include <set>

namespace websocketpp {
    
/// endpoint_base provides core functionality that needs to be constructed
/// before endpoint policy classes are constructed.
class endpoint_base {
protected:
    /// Start the run method of the endpoint's io_service object.
    void run_internal() {
        for (;;) {
            try {
                m_io_service.run();
                break;
            } catch (const std::exception & e) {
                throw e;
            }
        }
    }
    
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
   public role< endpoint<role,socket,logger> >,
   public socket< endpoint<role,socket,logger> >
{
public:
    /// Type of the traits class that stores endpoint related types.
    typedef endpoint_traits< endpoint<role,socket,logger> > traits;
    
    /// The type of the endpoint itself.
    typedef typename traits::type type;
    /// The type of a shared pointer to the endpoint.
    typedef typename traits::ptr ptr;
    /// The type of the role policy.
    typedef typename traits::role_type role_type;
    /// The type of the socket policy.
    typedef typename traits::socket_type socket_type;
    /// The type of the access logger based on the logger policy.
    typedef typename traits::alogger_type alogger_type;
    typedef typename traits::alogger_ptr alogger_ptr;
    /// The type of the error logger based on the logger policy.
    typedef typename traits::elogger_type elogger_type;
    typedef typename traits::elogger_ptr elogger_ptr;
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
#ifdef _WEBSOCKETPP_CPP11_FRIEND_
    // Highly simplified and preferred C++11 version:
    friend role_type;
    friend socket_type;
    friend connection_type;
#else
    friend class role< endpoint<role,socket> >;
    friend class socket< endpoint<role,socket> >;
    friend class connection<type,role< type >::template connection,socket< type >::template connection>;
#endif
    
    
    /// Construct an endpoint.
    /**
     * This constructor creates an endpoint and registers the default connection
     * handler.
     * 
     * @param handler A shared_ptr to the handler to use as the default handler
     * when creating new connections.
     */
    explicit endpoint(handler_ptr handler) 
     : role_type(endpoint_base::m_io_service)
     , socket_type(endpoint_base::m_io_service)
     , m_handler(handler)
     , m_read_threshold(DEFAULT_READ_THRESHOLD)
     , m_silent_close(DEFAULT_SILENT_CLOSE)
     , m_state(IDLE)
     , m_alog(new alogger_type())
     , m_elog(new elogger_type())
     , m_pool(new message::pool<message::data>(1000))
     , m_pool_control(new message::pool<message::data>(SIZE_MAX))
    {
        m_pool->set_callback(boost::bind(&type::on_new_message,this));
    }
    
    /// Destroy an endpoint
    ~endpoint() {
        m_alog->at(log::alevel::DEVEL) << "Endpoint destructor called" << log::endl;
        // Tell the memory pool we don't want to be notified about newly freed
        // messages any more (because we wont be here)
        m_pool->set_callback(NULL);
        
        // Detach any connections that are still alive at this point
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        typename std::set<connection_ptr>::iterator it;
        
        while (!m_connections.empty()) {
            remove_connection(*m_connections.begin());
        }
        m_alog->at(log::alevel::DEVEL) << "Endpoint destructor done" << log::endl;
    }
    
    // copy/assignment constructors require C++11
    // boost::noncopyable is being used in the meantime.
    // endpoint(endpoint const&) = delete;
    // endpoint& operator=(endpoint const&) = delete
    
    /// Returns a reference to the endpoint's access logger.
    /**
     * Visibility: public
     * State: Any
     * Concurrency: Callable from anywhere
     * 
     * @return A reference to the endpoint's access logger. See @ref logger
     * for more details about WebSocket++ logging policy classes.
     *
     * @par Example
     * To print a message to the access log of endpoint e at access level DEVEL:
     * @code
     * e.alog().at(log::alevel::DEVEL) << "message" << log::endl;
     * @endcode
     */
    alogger_type& alog() {
        return *m_alog;
    }
    alogger_ptr alog_ptr() {
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
        return *m_elog;
    }
    elogger_ptr elog_ptr() {
        return m_elog;
    }
    
    /// Get default handler
    /**
     * Visibility: public
     * State: valid always
     * Concurrency: callable from anywhere
     * 
     * @return A pointer to the default handler
     */
    handler_ptr get_handler() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        return m_handler;
    }
    
    /// Sets the default handler to be used for future connections
    /**
     * Does not affect existing connections.
     * 
     * @param new_handler A shared pointer to the new default handler. Must not
     * be NULL.
     */
    void set_handler(handler_ptr new_handler) {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        if (!new_handler) {
            elog().at(log::elevel::FATAL) 
                << "Tried to switch to a NULL handler." << log::endl;
            throw websocketpp::exception("TODO: handlers can't be null");
        }
        
        m_handler = new_handler;
    }
    
    /// Set endpoint read threshold
    /**
     * Sets the default read threshold value that will be passed to new connections. 
     * Changing this value will only affect new connections, not existing ones. The read
     * threshold represents the largest block of payload bytes that will be processed in
     * a single async read. Lower values may experience better callback latency at the 
     * expense of additional ASIO context switching overhead. This value also affects the
     * maximum number of bytes to be buffered before performing utf8 and other streaming
     * validation.
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
    
    /// Get endpoint read threshold
    /**
     * Returns the endpoint read threshold. See set_read_threshold for more information
     * about the read threshold.
     * 
     * Visibility: public
     * State: valid always
     * Concurrency: callable from anywhere
     * 
     * @return Size of the threshold in bytes
     * @see set_read_threshold()
     */
    size_t get_read_threshold() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        return m_read_threshold;
    }
    
    /// Set connection silent close setting
    /**
     * Silent close suppresses the return of detailed connection close information during
     * the closing handshake. This information is critically useful for debugging but may
     * be undesirable for security reasons for some production environments. Close reasons
     * could be used to by an attacker to confirm that the implementation is out of 
     * resources or be used to identify the WebSocket library in use.
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
    
    /// Cleanly closes all websocket connections
    /**
     * Sends a close signal to every connection with the specified code and 
     * reason. The default code is 1001/Going Away and the default reason is
     * blank. 
     * 
     * @param code The WebSocket close code to send to remote clients as the
     * reason that the connection is being closed.
     * @param reason The WebSocket close reason to send to remote clients as the
     * text reason that the connection is being closed. Must be valid UTF-8.
     */
    void close_all(close::status::value code = close::status::GOING_AWAY, 
                   const std::string& reason = "")
    {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        m_alog->at(log::alevel::ENDPOINT) 
        << "Endpoint received signal to close all connections cleanly with code " 
        << code << " and reason " << reason << log::endl;
        
        // TODO: is there a more elegant way to do this? In some code paths 
        // close can call terminate immediately which removes the connection
        // from m_connections, invalidating the iterator.
        typename std::set<connection_ptr>::iterator it;
        
        for (it = m_connections.begin(); it != m_connections.end();) {
            const connection_ptr con = *it++;
            con->close(code,reason);
        }
    }
    
    /// Stop the endpoint's ASIO loop
    /**
     * Signals the endpoint to call the io_service stop member function. If 
     * clean is true the endpoint will be put into an intermediate state where
     * it signals all connections to close cleanly and only calls stop once that
     * process is complete. Otherwise stop is called immediately and all 
     * io_service operations will be aborted.
     * 
     * If clean is true stop will use code and reason for the close code and 
     * close reason when it closes open connections. The default code is 
     * 1001/Going Away and the default reason is blank.
     * 
     * Visibility: public
     * State: Valid from RUNNING only
     * Concurrency: Callable from anywhere
     * 
     * @param clean Whether or not to wait until all connections have been
     * cleanly closed to stop io_service operations.
     * @param code The WebSocket close code to send to remote clients as the
     * reason that the connection is being closed.
     * @param reason The WebSocket close reason to send to remote clients as the
     * text reason that the connection is being closed. Must be valid UTF-8.
     */
    void stop(bool clean = true, 
              close::status::value code = close::status::GOING_AWAY, 
              const std::string& reason = "")
    {        
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        if (clean) {
            m_alog->at(log::alevel::ENDPOINT) 
                << "Endpoint is stopping cleanly" << log::endl;
            
            m_state = STOPPING;
            close_all(code,reason);
        } else {
            m_alog->at(log::alevel::ENDPOINT) 
                << "Endpoint is stopping immediately" << log::endl;
            
            endpoint_base::m_io_service.stop();
            m_state = STOPPED;
        }
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
     * If the endpoint is in a state where it is trying to stop or has already
     * stopped an empty shared pointer is returned.
     * 
     * Visibility: protected
     * State: Always valid, behavior differs based on state
     * Concurrency: Callable from anywhere
     *
     * @return A shared pointer to the newly created connection or an empty
     * shared pointer if one could not be created.
     */
    connection_ptr create_connection() {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        if (m_state == STOPPING || m_state == STOPPED) {
            return connection_ptr();
        }
        
        connection_ptr new_connection(new connection_type(*this,m_handler));
        m_connections.insert(new_connection);
        
        m_alog->at(log::alevel::DEVEL) << "Connection created: count is now: " 
                                       << m_connections.size() << log::endl;
        
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
     * Visibility: protected
     * State: Always valid, behavior differs based on state
     * Concurrency: Callable from anywhere
     * 
     * @param con A shared pointer to a connection created by this endpoint.
     */
    void remove_connection(connection_ptr con) {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        // TODO: is this safe to use?
        // Detaching signals to the connection that the endpoint is no longer aware of it
        // and it is no longer safe to assume the endpoint exists.
        con->detach();
        
        m_connections.erase(con);
        
        m_alog->at(log::alevel::DEVEL) << "Connection removed: count is now: " 
                                       << m_connections.size() << log::endl;
        
        if (m_state == STOPPING && m_connections.empty()) {
            // If we are in the process of stopping and have reached zero
            // connections stop the io_service.
            m_alog->at(log::alevel::ENDPOINT) 
                << "Endpoint has reached zero connections in STOPPING state. Stopping io_service now." 
                << log::endl;
            stop(false);
        }
    }
    
    /// Gets a shared pointer to a read/write data message.
    // TODO: thread safety
    message::data::ptr get_data_message() {
		return m_pool->get();
	}
    
    /// Gets a shared pointer to a read/write control message.
    // TODO: thread safety
    message::data::ptr get_control_message() {
		return m_pool_control->get();
	}
    
    /// Asks the endpoint to restart this connection's handle_read_frame loop
    /// when there are avaliable data messages.
    void wait(connection_ptr con) {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        m_read_waiting.push(con);
        m_alog->at(log::alevel::DEVEL) << "connection " << con << " is waiting. " << m_read_waiting.size() << log::endl;
    }
    
    /// Message pool callback indicating that there is a free data message
    /// avaliable. Causes one waiting connection to get restarted.
    void on_new_message() {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        
        if (!m_read_waiting.empty()) {
            connection_ptr next = m_read_waiting.front();
            
            m_alog->at(log::alevel::DEVEL) << "Waking connection " << next << ". " << m_read_waiting.size()-1 << log::endl;
            
            (*next).handle_read_frame(boost::system::error_code());
            m_read_waiting.pop();
            
            
        }
    }
private:
    enum state {
        IDLE = 0,
        RUNNING = 1,
        STOPPING = 2,
        STOPPED = 3
    };
    
    // default settings to pass to connections
    handler_ptr                 m_handler;
    size_t                      m_read_threshold;
    bool                        m_silent_close;
    
    // other stuff
    state                       m_state;
    
    std::set<connection_ptr>    m_connections;
    alogger_ptr                 m_alog;
    elogger_ptr                 m_elog;
    
    // resource pools for read/write message buffers
    message::pool<message::data>::ptr   m_pool;
    message::pool<message::data>::ptr   m_pool_control;
    std::queue<connection_ptr>          m_read_waiting;
    
    // concurrency support
    mutable boost::recursive_mutex      m_lock;
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
    typedef boost::shared_ptr<type> ptr;
    
    /// The type of the role policy.
    typedef role< type > role_type;
    /// The type of the socket policy.
    typedef socket< type > socket_type;
    
    /// The type of the access logger based on the logger policy.
    typedef logger<log::alevel::value> alogger_type;
    typedef boost::shared_ptr<alogger_type> alogger_ptr;
    /// The type of the error logger based on the logger policy.
    typedef logger<log::elevel::value> elogger_type;
    typedef boost::shared_ptr<elogger_type> elogger_ptr;
    
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
        // convenience typedefs for use in end application handlers.
        // TODO: figure out how to not duplicate the definition of connection_ptr
        typedef boost::shared_ptr<handler> ptr;
        typedef typename connection_type::ptr connection_ptr;
        typedef typename message::data::ptr message_ptr;
        
        virtual ~handler() {}
        
        /// on_load is the first callback called for a handler after a new
        /// connection has been transferred to it mid flight.
        /**
         * @param connection A shared pointer to the connection that was transferred
         * @param old_handler A shared pointer to the previous handler
         */
        virtual void on_load(connection_ptr con, handler_ptr old_handler) {}
        /// on_unload is the last callback called for a handler before control
        /// of a connection is handed over to a new handler mid flight.
        /**
         * @param connection A shared pointer to the connection being transferred
         * @param old_handler A shared pointer to the new handler
         */
        virtual void on_unload(connection_ptr con, handler_ptr new_handler) {}
    };
    
};

} // namespace websocketpp

#endif // WEBSOCKETPP_ENDPOINT_HPP
