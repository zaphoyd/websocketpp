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

/**
 * 
 * 
 * 
 * 
 * 
From Endpoint
- get new connection


*/

template <typename concurrency, typename security>
class endpoint : public security {
public:
	typedef endpoint<concurrency,security> type;
		
	typedef asio::connection<security> con_policy;
	
	//typedef lib::shared_ptr<boost::asio::io_service> io_service_ptr;
	typedef boost::asio::io_service* io_service_ptr;
	typedef lib::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor_ptr;
		
	// generate and manage our own io_service
	explicit endpoint() 
	  : m_external_io_service(false)
	  , m_state(UNINITIALIZED)
	{
		std::cout << "asio transport constructor" << std::endl; 
	}
	
	~endpoint() {
		// clean up our io_service if we were initialized with an internal one.
		if (m_state != UNINITIALIZED && m_external_io_service) {
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
	void init_asio(io_service_ptr ptr, bool external = true) {
		if (m_state != UNINITIALIZED) {
			// TODO: throw invalid state
			throw;
		}
		m_io_service = ptr;
        m_external_io_service = external;
		m_acceptor.reset(new boost::asio::ip::tcp::acceptor(*m_io_service));
		m_state = READY;
	}
	
	/// Initialize asio transport with internal io_service
	/**
	 * @see init_asio(io_service_ptr ptr)
	 */
	void init_asio() {
		init_asio(new boost::asio::io_service(),false);
	}
	
    /// Sets the tcp init handler
    /**
     * The tcp init handler is called after the tcp connection has been 
     * established.
     *
     * @see WebSocket++ handler documentation for more information about
     * handlers.
     */

	// listen manually
	void listen(const boost::asio::ip::tcp::endpoint& e) {
		if (m_state != READY) {
			// TODO
			std::cout << "asio::listen called from the wrong state" << std::endl;
			throw;
		}
		m_acceptor->open(e.protocol());
        m_acceptor->set_option(boost::asio::socket_base::reuse_address(true));
        m_acceptor->bind(e);
        m_acceptor->listen();
        m_state = LISTENING;
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
	template <typename connection_ptr>
	void async_accept(connection_ptr con, accept_handler callback) {
		if (m_state != LISTENING) {
			// TODO: throw invalid state
			std::cout << "asio::async_accept called from the wrong state" << std::endl;
			throw;
		}
		
		std::cout << "call async accept" << std::endl;
		
		// TEMP
		m_acceptor->async_accept(
			con->get_raw_socket(),
			lib::bind(
				&type::handle_accept<connection_ptr>,
				this,
				con,
				callback,
				lib::placeholders::_1
			)
		);
	}

	/// wraps the run method of the internal io_service object
	void run() {
		m_io_service->run();
	}
	
	/// wraps the stop method of the internal io_service object
	void stop() {
		m_io_service->stop();
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
protected:
	template <typename connection_ptr>
	void handle_accept(connection_ptr con, accept_handler callback, const boost::system::error_code& error) {
		if (error) {
			con->terminate();
			// TODO: Better translation of errors at this point
			callback(make_error_code(error::pass_through));
		}
		
		con->start();
		callback(lib::error_code());
	}
	
	bool is_listening() const {
		return (m_state == LISTENING);
	}
	
	/// Initialize a connection
	/**
	 * init(connection_ptr) is called by an endpoint once for each newly created
	 * connection. It's purpose is to give the transport policy the chance to
	 * perform any transport specific initialization that couldn't be done via
	 * the default constructor.
	 */
	template <typename connection_ptr>
	void init(connection_ptr con) {
		// init tls?
		con->init_asio(m_io_service);
        con->set_tcp_init_handler(m_tcp_init_handler);
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
	
	// Transport state
	state				m_state;
};

} // namespace asio
} // namespace transport
} // namespace websocketpp

#endif // WEBSOCKETPP_TRANSPORT_ASIO_HPP
