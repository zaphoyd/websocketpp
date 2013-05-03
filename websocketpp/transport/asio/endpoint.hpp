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

#ifndef WEBSOCKETPP_TRANSPORT_ASIO_HPP
#define WEBSOCKETPP_TRANSPORT_ASIO_HPP

#include <websocketpp/common/functional.hpp>
#include <websocketpp/transport/base/endpoint.hpp>
#include <websocketpp/transport/asio/connection.hpp>
#include <websocketpp/transport/asio/security/none.hpp>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/system/error_code.hpp>
	
#include <iostream>

namespace websocketpp {
namespace transport {
namespace asio {

/// Boost Asio based endpoint transport component
/**
 * transport::asio::endpoint impliments an endpoint transport component using
 * Boost ASIO.
 */
template <typename config>
class endpoint : public config::socket_type {
public:
    /// Type of this endpoint transport component
	typedef endpoint<config> type;

    /// Type of the concurrency policy
    typedef typename config::concurrency_type concurrency_type;
    /// Type of the socket policy
    typedef typename config::socket_type socket_type;
    /// Type of the error logging policy
    typedef typename config::elog_type elog_type;
    /// Type of the access logging policy
    typedef typename config::alog_type alog_type;

    /// Type of the socket connection component
    typedef typename socket_type::socket_con_type socket_con_type;
    /// Type of a shared pointer to the socket connection component
    typedef typename socket_con_type::ptr socket_con_ptr;
    
    /// Type of the connection transport component associated with this
    /// endpoint transport component
	typedef asio::connection<config> transport_con_type;
    /// Type of a shared pointer to the connection transport component
    /// associated with this endpoint transport component
    typedef typename transport_con_type::ptr transport_con_ptr;

    /// Type of a pointer to the ASIO io_service being used
	typedef boost::asio::io_service* io_service_ptr;
    /// Type of a shared pointer to the acceptor being used
	typedef lib::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor_ptr;
    /// Type of a shared pointer to the resolver being used
	typedef lib::shared_ptr<boost::asio::ip::tcp::resolver> resolver_ptr;

	// generate and manage our own io_service
	explicit endpoint() 
	  : m_external_io_service(false)
	  , m_state(UNINITIALIZED)
	{
		//std::cout << "transport::asio::endpoint constructor" << std::endl; 
	}
	
	~endpoint() {
		// clean up our io_service if we were initialized with an internal one.
        m_acceptor.reset();
		if (m_state != UNINITIALIZED && !m_external_io_service) {
			delete m_io_service;
		}
	}

	/// transport::asio objects are moveable but not copyable or assignable.
	/// The following code sets this situation up based on whether or not we
	/// have C++11 support or not
#ifdef _WEBSOCKETPP_DELETED_FUNCTIONS_
	endpoint(const endpoint& src) = delete;
	endpoint& operator= (const endpoint & rhs) = delete;
#else
private:
	endpoint(const endpoint& src);
	endpoint& operator= (const endpoint & rhs);
public:
#endif

#ifdef _WEBSOCKETPP_RVALUE_REFERENCES_
	endpoint (endpoint&& src)
	  : m_io_service(src.m_io_service)
	  , m_external_io_service(src.m_external_io_service)
	  , m_acceptor(src.m_acceptor)
	  , m_state(src.m_state)
	{
		src.m_io_service = NULL;
		src.m_external_io_service = false;
		src.m_acceptor = NULL;
		src.m_state = UNINITIALIZED;
	}
	
	endpoint& operator= (const endpoint && rhs) {
		if (this != &rhs) {
			m_io_service = rhs.m_io_service;
			m_external_io_service = rhs.m_external_io_service;
			m_acceptor = rhs.m_acceptor;
			m_state = rhs.m_state;
			
			rhs.m_io_service = NULL;
			rhs.m_external_io_service = false;
			rhs.m_acceptor = NULL;
			rhs.m_state = UNINITIALIZED;
		}
		return *this;
	}
#endif
	
	/// initialize asio transport with external io_service
	/**
	 * Initialize the ASIO transport policy for this endpoint using the 
	 * io_service object. asio_init must be called exactly once on any endpoint
	 * that uses transport::asio before it can be used.
	 *
	 * Calling init_asio shifts the internal state from UNINITIALIZED to READY
	 */
	void init_asio(io_service_ptr ptr) {
		if (m_state != UNINITIALIZED) {
			// TODO: throw invalid state
			m_elog->write(log::elevel::library,
                "asio::init_asio called from the wrong state");
			throw;
		}
		
		m_alog->write(log::alevel::devel,"asio::init_asio");
		
		m_io_service = ptr;
        m_external_io_service = true;
		m_acceptor.reset(new boost::asio::ip::tcp::acceptor(*m_io_service));
		m_state = READY;
	}
	
	/// Initialize asio transport with internal io_service
	/**
	 * @see init_asio(io_service_ptr ptr)
	 */
	void init_asio() {
		init_asio(new boost::asio::io_service());
        m_external_io_service = false;
	}
	
    /// Sets the tcp init handler
    /**
     * The tcp init handler is called after the tcp connection has been 
     * established.
     *
     * @see WebSocket++ handler documentation for more information about
     * handlers.
     */
    void set_tcp_init_handler(tcp_init_handler h) {
        m_tcp_init_handler = h;
    }

	// listen manually
	void listen(const boost::asio::ip::tcp::endpoint& e) {
		if (m_state != READY) {
			// TODO
            m_elog->write(log::elevel::library,
                "asio::listen called from the wrong state");
			throw;
		}
		
		m_alog->write(log::alevel::devel,"asio::listen");
		
		m_acceptor->open(e.protocol());
        m_acceptor->set_option(boost::asio::socket_base::reuse_address(true));
        m_acceptor->bind(e);
        m_acceptor->listen();
        m_state = LISTENING;
        
        m_alog->write(log::alevel::devel,"mark");
	}
	
	void cancel() {
		if (m_state != LISTENING) {
			// TODO
			throw;
		}
		
		// TODO: figure out if this is a good way to stop listening.
		m_acceptor->cancel();
		m_acceptor->close();
	}

	// Accept the next connection attempt via m_acceptor and assign it to con.
	// callback is called 
	void async_accept(transport_con_ptr tcon, accept_handler callback) {
		if (m_state != LISTENING) {
			// TODO: throw invalid state
            m_elog->write(log::elevel::library,
                "asio::async_accept called from the wrong state");
			throw;
		}
		
        m_alog->write(log::alevel::devel, "asio::async_accept");
		
		// TEMP
		m_acceptor->async_accept(
			tcon->get_raw_socket(),
			lib::bind(
				&type::handle_accept,
				this,
				tcon->get_handle(),
				callback,
				lib::placeholders::_1
			)
		);
	}

	/// wraps the run method of the internal io_service object
	std::size_t run() {
		return m_io_service->run();
	}
	
	/// wraps the stop method of the internal io_service object
	void stop() {
		m_io_service->stop();
	}
	
	/// wraps the poll method of the internal io_service object
	std::size_t poll() {
		return m_io_service->poll();
	}
	
	/// wraps the poll_one method of the internal io_service object
	std::size_t poll_one() {
		return m_io_service->poll_one();
	}
	
	/// wraps the reset method of the internal io_service object
	void reset() {
		m_io_service->reset();
	}
	
	/// wraps the stopped method of the internal io_service object
	bool stopped() const {
		return m_io_service->stopped();
	}

	// convenience methods
	template <typename InternetProtocol> 
    void listen(const InternetProtocol &internet_protocol, uint16_t port) {
        boost::asio::ip::tcp::endpoint e(internet_protocol, port);
        listen(e);
    }
	
	void listen(uint16_t port) {
		listen(boost::asio::ip::tcp::v6(), port);
	}
	
	void listen(const std::string &host, const std::string &service) {
		boost::asio::ip::tcp::resolver resolver(*m_io_service);
		boost::asio::ip::tcp::resolver::query query(host, service);
		boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
		boost::asio::ip::tcp::resolver::iterator end;
		if (endpoint_iterator == end) {
			throw std::invalid_argument("Can't resolve host/service to listen");
		}
		listen(*endpoint_iterator);
	}
	
	typedef lib::shared_ptr<boost::asio::deadline_timer> timer_ptr;
    
    timer_ptr set_timer(long duration, timer_handler handler) {
        timer_ptr timer(new boost::asio::deadline_timer(*m_io_service)); 
        timer->expires_from_now(boost::posix_time::milliseconds(duration));
        timer->async_wait(lib::bind(&type::timer_handler, this, handler, 
            lib::placeholders::_1));
        return timer;
    }
    
    void timer_handler(timer_handler h, const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted) {
            h(make_error_code(transport::error::operation_aborted));
        } else if (ec) {
            std::stringstream s;
            s << "asio async_wait error::pass_through"
              << "Original Error: " << ec << " (" << ec.message() << ")";
            m_elog->write(log::elevel::devel,s.str());
            h(make_error_code(transport::error::pass_through));
        } else {
            h(lib::error_code());
        }
    }
    
    boost::asio::io_service& get_io_service() {
    	return *m_io_service;
    }
    
    bool is_secure() const {
        return socket_type::is_secure();
    }
protected:
    /// Initialize logging
    /**
     * The loggers are located in the main endpoint class. As such, the 
     * transport doesn't have direct access to them. This method is called
     * by the endpoint constructor to allow shared logging from the transport
     * component. These are raw pointers to member variables of the endpoint.
     * In particular, they cannot be used in the transport constructor as they
     * haven't been constructed yet, and cannot be used in the transport 
     * destructor as they will have been destroyed by then.
     */
    void init_logging(elog_type* e, alog_type* a) {
        m_elog = e;
        m_alog = a;
    }

	void handle_accept(connection_hdl hdl, accept_handler callback,
        const boost::system::error_code& error)
    {
		if (error) {
			//con->terminate();
			// TODO: Better translation of errors at this point
			callback(hdl,make_error_code(error::pass_through));
			return;
		}
		
		//con->start();
		callback(hdl,lib::error_code());
	}
	
	/// Initiate a new connection
	// TODO: there have to be some more failure conditions here
    void async_connect(transport_con_ptr tcon, uri_ptr u, connect_handler cb) {
        using namespace boost::asio::ip;
        
        // Create a resolver
        if (!m_resolver) {
            m_resolver.reset(new boost::asio::ip::tcp::resolver(*m_io_service));
        }
        
        std::string proxy = tcon->get_proxy();
        std::string host;
        std::string port;
        
        if (proxy.empty()) {
            host = u->get_host();
            port = u->get_port_str();
        } else {
            try {
                lib::error_code ec;

                uri_ptr pu(new uri(proxy));
                ec = tcon->proxy_init(u->get_host_port());
                if (ec) {
                    cb(tcon->get_handle(),ec);
                    return;
                }

                host = pu->get_host();
                port = pu->get_port_str();
            } catch (uri_exception) {
                cb(tcon->get_handle(),make_error_code(error::proxy_invalid));
                return;
            }
        }
        
        tcp::resolver::query query(host,port);
        
        m_resolver->async_resolve(
            query,
            lib::bind(
                &type::handle_resolve,
                this,
                tcon,
                cb,
                lib::placeholders::_1,
                lib::placeholders::_2
            )
        );
    }
    
    void handle_resolve(transport_con_ptr tcon, connect_handler callback,
        const boost::system::error_code& ec, 
        boost::asio::ip::tcp::resolver::iterator iterator)
    {
		if (ec) {
			//con->terminate();
			// TODO: Better translation of errors at this point
			std::stringstream s;
            s << "asio async_resolve error::pass_through: "
              << "Original Error: " << ec << " (" << ec.message() << ")";
            m_elog->write(log::elevel::info,s.str());
			callback(tcon->get_handle(),make_error_code(error::pass_through));
			return;
		}
		
		boost::asio::async_connect(
            tcon->get_raw_socket(),
            iterator,
            lib::bind(
                &type::handle_connect,
                this, // shared from this?
                tcon,
                callback,
				lib::placeholders::_1
            )
        );
	}
    
    void handle_connect(transport_con_ptr tcon, connect_handler callback,
        const boost::system::error_code& ec)
    {
		if (ec) {
			//con->terminate();
			// TODO: Better translation of errors at this point
			std::stringstream s;
            s << "asio async_connect error::pass_through: "
              << "Original Error: " << ec << " (" << ec.message() << ")";
            m_elog->write(log::elevel::info,s.str());
			callback(tcon->get_handle(),make_error_code(error::pass_through));
			return;
		}
		
		callback(tcon->get_handle(),lib::error_code());
	}
	
	bool is_listening() const {
		return (m_state == LISTENING);
	}
	
	/// Initialize a connection
	/**
	 * init is called by an endpoint once for each newly created connection. 
     * It's purpose is to give the transport policy the chance to perform any 
     * transport specific initialization that couldn't be done via the default 
     * constructor.
     *
     * @param tcon A pointer to the transport portion of the connection.
	 */
	void init(transport_con_ptr tcon) {
        m_alog->write(log::alevel::devel, "transport::asio::init");

        // Initialize the connection socket component
        socket_type::init(lib::static_pointer_cast<socket_con_type,
            transport_con_type>(tcon));

		tcon->init_asio(m_io_service);
        tcon->set_tcp_init_handler(m_tcp_init_handler);
	}
private:
	enum state {
		UNINITIALIZED = 0,
		READY = 1,
		LISTENING = 2
	};
    
    // Handlers
    tcp_init_handler    m_tcp_init_handler;

	// Network Resources
	io_service_ptr		m_io_service;
	bool				m_external_io_service;
	acceptor_ptr		m_acceptor;
	resolver_ptr        m_resolver;
	
    elog_type* m_elog;
    alog_type* m_alog;

	// Transport state
	state				m_state;
};

} // namespace asio
} // namespace transport
} // namespace websocketpp

#endif // WEBSOCKETPP_TRANSPORT_ASIO_HPP
