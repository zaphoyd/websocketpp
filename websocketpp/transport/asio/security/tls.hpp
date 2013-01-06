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

#ifndef WEBSOCKETPP_TRANSPORT_SECURITY_TLS_HPP
#define WEBSOCKETPP_TRANSPORT_SECURITY_TLS_HPP

#include <websocketpp/transport/asio/security/base.hpp>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/system/error_code.hpp>

#include <iostream>
#include <string>

namespace websocketpp {
namespace transport {
namespace security {

class tls {
public:
	typedef tls type;
	
	typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket_type;
	typedef boost::asio::io_service* io_service_ptr;
    typedef lib::shared_ptr<socket_type> socket_ptr;
	typedef lib::shared_ptr<boost::asio::ssl::context> context_ptr;
	typedef lib::shared_ptr<boost::asio::deadline_timer> timer_ptr;
	
	typedef boost::system::error_code boost_error;
	
	class handler_interface {
	public:
		virtual void on_socket_init(socket_type::lowest_layer_type& socket) {};
		virtual lib::shared_ptr<boost::asio::ssl::context> on_tls_init() = 0;
	};
	
	typedef lib::shared_ptr<handler_interface> handler_ptr;
	
	
	explicit tls()
	{
		std::cout << "transport::security::tls constructor" << std::endl; 
	}
	
	bool is_secure() const {
	    return true;
	}
	
	/// Retrieve a pointer to the underlying socket
	/**
	 * This is used internally. It can also be used to set socket options, etc
	 */
	socket_type::lowest_layer_type& get_raw_socket() {
		return m_socket->lowest_layer();
	}
	
	/// Retrieve a pointer to the wrapped socket
	/**
	 * This is used internally.
	 */
	socket_type& get_socket() {
		return *m_socket;
	}
protected:
	/// Perform one time initializations
	/**
	 * init_asio is called once immediately after construction to initialize
	 * boost::asio components to the io_service
	 *
	 * @param service A pointer to the endpoint's io_service
	 * @param strand A shared pointer to the connection's asio strand
	 * @param is_server Whether or not the endpoint is a server or not.
	 */
    lib::error_code init_asio (io_service_ptr service, bool is_server) {
    	m_context = m_handler->on_tls_init();
    	
    	if (!m_context) {
			return make_error(error::invalid_tls_context);
		}
    	
    	m_socket.reset(new socket_type(*service,*m_context));
    	
    	m_timer.reset(new boost::asio::deadline_timer(
    		*service,
    		boost::posix_time::seconds(0))
    	);
    	
    	m_io_service = service;
    	m_is_server = is_server;
    	
    	return lib::error_code();
    }
	
	/// Initialize security policy for reading
	void init(init_handler callback) {
		m_handler->on_socket_init(get_raw_socket());
		
		// register timeout
		m_timer->expires_from_now(boost::posix_time::milliseconds(5000));
		// TEMP
		m_timer->async_wait(
            lib::bind(
                &type::handle_timeout,
                this,
                callback,
                lib::placeholders::_1
            )
        );
		
		// TLS handshake
		m_socket->async_handshake(
			get_handshake_type(),
			lib::bind(
				&type::handle_init,
				this,
				callback,
				lib::placeholders::_1
			)
		);
	}
	
	void handle_timeout(init_handler callback, const 
		boost::system::error_code& error)
	{
		if (error) {
			if (error.value() == boost::asio::error::operation_aborted) {
				// The timer was cancelled because the handshake succeeded.
				return;
			}
			
			// Some other ASIO error, pass it through
			// TODO: make this error pass through better
    		callback(make_error(error::pass_through));
    		return;
    	}
    	
    	callback(make_error(error::tls_handshake_timeout));
	}
	
	void handle_init(init_handler callback, const 
		boost::system::error_code& error)
	{
		/// stop waiting for our handshake timer.
		m_timer->cancel();
		
		if (error) {
			// TODO: make this error pass through better
    		callback(make_error(error::pass_through));
    		return;
    	}
		
		callback(lib::error_code());
	}
	
	void set_handler(handler_ptr new_handler) {
		m_handler = new_handler;
	}

    void shutdown() {
        boost::system::error_code ec;
        m_socket->shutdown(ec);

        // TODO: error handling
    }
private:
	socket_type::handshake_type get_handshake_type() {
        if (m_is_server) {
            return boost::asio::ssl::stream_base::server;
        } else {
            return boost::asio::ssl::stream_base::client;
        }
    }
	
	io_service_ptr	m_io_service;
	context_ptr		m_context;
	socket_ptr		m_socket;
	timer_ptr		m_timer;
	bool			m_is_server;
	handler_ptr		m_handler;
};

} // namespace security
} // namespace transport
} // namespace websocketpp

#endif // WEBSOCKETPP_TRANSPORT_SECURITY_TLS_HPP
