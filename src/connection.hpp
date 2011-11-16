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
	template <class> class socket>
class connection 
 : public socket< connection<endpoint,socket> >,
   public boost::enable_shared_from_this< connection<endpoint, socket> >
{
public:
	typedef connection_traits< connection<endpoint,socket> > traits;
	
	// get types that we need from our traits class
	typedef typename traits::type type;
	typedef typename traits::socket_type socket_type;
	
	typedef endpoint endpoint_type;
	
	connection(endpoint_type& e) 
	 : socket_type(e),
	   m_endpoint(e)
	{
		std::cout << "setup connection" << std::endl;
	}
	
	void websocket_handshake() {
		std::cout << "Websocket Handshake" << std::endl;
		
		socket_type::get_socket().async_read_some(
			boost::asio::buffer(m_data, 512),
			boost::bind(
				&type::handle_read,
				type::shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred
			)
		);
		
		this->websocket_messages(); // temp
	}
	
	// temporary
	void websocket_messages() {
		std::cout << "Websocket Messages" << std::endl;
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
			m_endpoint.remove_connection(type::shared_from_this());
		}
	}
protected:
	endpoint_type& m_endpoint;
	char m_data[512]; // temporary
};

// connection related types that it and its policy classes need.
template <typename endpoint,template <class> class socket>
struct connection_traits< connection<endpoint, socket> > {
	// type of the connection itself
	typedef connection<endpoint,socket> type;
	
	// types of the connection policies
	typedef endpoint endpoint_type;
	typedef socket< type > socket_type;
};
	
} // namespace websocketpp

#endif // WEBSOCKETPP_CONNECTION_HPP
