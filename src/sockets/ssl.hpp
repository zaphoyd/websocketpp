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

#ifndef WEBSOCKETPP_SOCKET_SSL_HPP
#define WEBSOCKETPP_SOCKET_SSL_HPP

#include "socket_base.hpp"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>

#include <iostream>

namespace websocketpp {
namespace socket {

template <typename endpoint_type>
class ssl {
public:
	typedef ssl<endpoint_type> type;
	typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket;
	
	std::string get_password() const {
		return "test";
	}
	
	// should be private friended
	boost::asio::io_service& get_io_service() {
		return m_io_service;
	}
	
	// should be private friended
	boost::asio::ssl::context& get_context() {
		return m_context;
	}
	
	// should be private friended?
	ssl_socket::handshake_type get_handshake_type() {
		if (static_cast< endpoint_type* >(this)->is_server()) {
			return boost::asio::ssl::stream_base::server;
		} else {
			return boost::asio::ssl::stream_base::client;
		}
	}
	
	// Connection specific details
	template <typename connection_type>
	class connection {
	public:
		// should these two be public or protected. If protected, how?
		ssl_socket::lowest_layer_type& get_raw_socket() {
			return m_socket.lowest_layer();
		}
		
		ssl_socket& get_socket() {
			return m_socket;
		}
	protected:
		connection(ssl<endpoint_type>& e) : m_socket(e.get_io_service(),e.get_context()),m_endpoint(e) {}
		
		void async_init(boost::function<void(const boost::system::error_code&)> callback) {
			m_socket.async_handshake(
				m_endpoint.get_handshake_type(),
				boost::bind(
					&connection<connection_type>::handle_init,
					this,
					callback,
					boost::asio::placeholders::error
				)
			);
		}
		
		void handle_init(socket_init_callback callback,const boost::system::error_code& error) {
			/*if (error) {
				std::cout << "SSL handshake error" << std::endl;
			} else {
				//static_cast< connection_type* >(this)->websocket_handshake();
				
			}*/
			callback(error);
		}
		
		bool shutdown() {
			boost::system::error_code ignored_ec;
			m_socket.shutdown(ignored_ec);
			
			if (ignored_ec) {
				return false;
			} else {
				return true;
			}
		}
	private:
		ssl_socket			m_socket;
		ssl<endpoint_type>&	m_endpoint;
	};
protected:
	ssl (boost::asio::io_service& m) : m_io_service(m),m_context(boost::asio::ssl::context::sslv23) {
		try {
			// this should all be in separate init functions
			m_context.set_options(boost::asio::ssl::context::default_workarounds |
								  boost::asio::ssl::context::no_sslv2 |
								  boost::asio::ssl::context::single_dh_use);
			m_context.set_password_callback(boost::bind(&type::get_password, this));
			m_context.use_certificate_chain_file("/Users/zaphoyd/Documents/websocketpp/src/ssl/server.pem");
			m_context.use_private_key_file("/Users/zaphoyd/Documents/websocketpp/src/ssl/server.pem", boost::asio::ssl::context::pem);
			m_context.use_tmp_dh_file("/Users/zaphoyd/Documents/websocketpp/src/ssl/dh512.pem");
		} catch (std::exception& e) {
			std::cout << e.what() << std::endl;
		}
	}
private:
	boost::asio::io_service&	m_io_service;
	boost::asio::ssl::context	m_context;
	ssl_socket::handshake_type	m_handshake_type;
};
	
} // namespace socket
} // namespace websocketpp

#endif // WEBSOCKETPP_SOCKET_SSL_HPP
