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

#ifndef WEBSOCKETPP_CONNECTION_HPP
#define WEBSOCKETPP_CONNECTION_HPP

//#include "endpoint.hpp"
#include "http/parser.hpp"
#include "logger.hpp"

#include "roles/server.hpp"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>

#include <iostream> // temporary?

namespace websocketpp {

template <typename T>
struct connection_traits;
	
template <
	typename endpoint,
	template <class> class role,
	template <class> class socket>
class connection 
 : public role< connection<endpoint,role,socket> >,
   public socket< connection<endpoint,role,socket> >,
   public boost::enable_shared_from_this< connection<endpoint,role,socket> >
{
public:
	typedef connection_traits< connection<endpoint,role,socket> > traits;
	
	// get types that we need from our traits class
	typedef typename traits::type type;
	typedef typename traits::role_type role_type;
	typedef typename traits::socket_type socket_type;
	
	typedef endpoint endpoint_type;
		
	// friends (would require C++11) this would enable connection::start to be 
	// protected instead of public.
	// friend typename endpoint_traits<endpoint_type>::role_type;
	
	connection(endpoint_type& e) 
	 : role_type(e),
	   socket_type(e),
	   m_endpoint(e) {}
	
	void start() {
		// initialize the socket.
		socket_type::async_init(
			boost::bind(
				&type::handle_socket_init,
				type::shared_from_this(),
				boost::asio::placeholders::error
			)
		);
	}
protected:	
	void handle_socket_init(const boost::system::error_code& error) {
		if (error) {
			m_endpoint.elog().at(log::elevel::ERROR) << "Connection initialization failed, error code: " << error << log::endl;
			this->terminate();
			return;
		}
		
		this->websocket_handshake();
	}
	
	void websocket_handshake() {
		m_endpoint.alog().at(log::alevel::DEVEL) << "Websocket Handshake" << log::endl;
		
		socket_type::get_socket().async_read_some(
			boost::asio::buffer(m_data, 512),
			boost::bind(
				&type::handle_read,
				type::shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred
			)
		);
	}
	
	void handle_read(const boost::system::error_code& error,
					 size_t bytes_transferred)
	{
		if (error) {
			std::cout << "read error" << std::endl;
		} else {
			std::cout << "read complete: " << m_data << std::endl;
			
			std::string r = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nbar";
			
			boost::asio::async_write(
				socket_type::get_socket(),
				boost::asio::buffer(r, r.size()),
				boost::bind(
					&type::handle_write,
					type::shared_from_this(),
					boost::asio::placeholders::error
				)
			);
		}
	}
	
	void handle_write(const boost::system::error_code& error) {
		if (error) {
			std::cout << "write error" << std::endl;
		} else {
			std::cout << "write successful" << std::endl;
			// end session
			this->terminate();
		}
	}
	
	// terminate cleans up a connection and removes it from the endpoint's 
	// connection list.
	void terminate() {
		// TODO: close socket cleanly
		m_endpoint.remove_connection(type::shared_from_this());
	}
protected:
	endpoint_type& m_endpoint;
	char m_data[512]; // temporary
	
	http::parser::request		m_request;
	http::parser::response		m_response;
};

// connection related types that it and its policy classes need.
template <
	typename endpoint,
	template <class> class role,
	template <class> class socket>
struct connection_traits< connection<endpoint,role,socket> > {
	// type of the connection itself
	typedef connection<endpoint,role,socket> type;
	
	// types of the connection policies
	typedef endpoint endpoint_type;
	typedef role< type > role_type;
	typedef socket< type > socket_type;
};
	
} // namespace websocketpp

#endif // WEBSOCKETPP_CONNECTION_HPP
