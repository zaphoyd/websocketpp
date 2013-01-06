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

#ifndef WEBSOCKETPP_TRANSPORT_SECURITY_NONE_HPP
#define WEBSOCKETPP_TRANSPORT_SECURITY_NONE_HPP

#include <websocketpp/transport/asio/security/base.hpp>

#include <boost/asio.hpp>

#include <iostream>
#include <string>

namespace websocketpp {
namespace transport {
namespace security {

class none {
public:
	typedef lib::shared_ptr<boost::asio::ip::tcp::socket> socket_ptr;
	typedef boost::asio::io_service* io_service_ptr;
	
	class handler_interface {
	public:
		virtual void on_socket_init(boost::asio::ip::tcp::socket& socket) {};
	};
	
	typedef lib::shared_ptr<handler_interface> handler_ptr;
	
	explicit none() : m_state(UNINITIALIZED)
	{
		std::cout << "transport::security::none constructor" << std::endl; 
	}
	
	bool is_secure() const {
	    return false;
	}
	
	/// Retrieve a pointer to the underlying socket
	/**
	 * This is used internally. It can also be used to set socket options, etc
	 */
    boost::asio::ip::tcp::socket& get_socket() {
    	return *m_socket;
    }
    
    /// Retrieve a pointer to the underlying socket
	/**
	 * This is used internally. It can also be used to set socket options, etc
	 */
    boost::asio::ip::tcp::socket& get_raw_socket() {
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
    	if (m_state != UNINITIALIZED) {
    		return make_error(error::invalid_state);
    	}
    	
    	m_socket.reset(new boost::asio::ip::tcp::socket(*service));
    	
    	m_state = READY;
    	
    	return lib::error_code();
    }
	
	/// Initialize security policy for reading
	void init(init_handler callback) {
		if (m_state != READY) {
	 		callback(make_error(error::invalid_state));
	 		return;
	 	}
	 	
	 	m_handler->on_socket_init(*m_socket);
	 	
	 	m_state = READING;
	 	
	 	callback(lib::error_code());
	}
	
	void set_handler(handler_ptr new_handler) {
		m_handler = new_handler;
	}

    void shutdown() {
        boost::system::error_code ec;
        m_socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both,ec);

        // TODO: handle errors
    }
private:
	enum state {
		UNINITIALIZED = 0,
		READY = 1,
		READING = 2
	};
	
	socket_ptr	m_socket;
	state		m_state;
	handler_ptr m_handler;
};

} // namespace security
} // namespace transport
} // namespace websocketpp

#endif // WEBSOCKETPP_TRANSPORT_SECURITY_NONE_HPP
