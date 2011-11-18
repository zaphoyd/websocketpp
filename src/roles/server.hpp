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

#ifndef WEBSOCKETPP_ROLE_SERVER_HPP
#define WEBSOCKETPP_ROLE_SERVER_HPP

#include "../endpoint.hpp"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>

#include <iostream>

namespace websocketpp {
namespace role {

template <class endpoint>
class server {
public:
	// Connection specific details
	template <typename connection_type>
	class connection {
	public:
		// Valid always
		std::string get_request_header(const std::string& key) const {}
		std::string get_origin() const {}
		
		// Valid for CONNECTING state
		void add_response_header(const std::string& key, const std::string& value) {};
		void replace_response_header(const std::string& key, const std::string& e) {};
		const std::vector<std::string>& get_subprotocols() const {};
		const std::vector<std::string>& get_extensions() const {};
		void select_subprotocol(const std::string& value) {};
		void select_extension(const std::string& value) {};
	protected:
		//connection(server<endpoint>& e) : m_endpoint(e) {}
		connection(endpoint& e) : m_endpoint(e) {}
		
		// initializes the websocket connection
		void async_init() {
			/*boost::asio::async_read_until(
				m_endpoint.socket(),
				m_buf,
				"\r\n\r\n",
				boost::bind(
					&connection_type::handle_read_request,
					connection_type::shared_from_this(),
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred
				)
			);*/
		}
		
		
		
	private:
		endpoint&					m_endpoint;
		
		http::parser::request		m_request;
		http::parser::response		m_response;
	};
	
	// handler interface callback class
	class handler {
		virtual void on_action() = 0;
	};
	
	typedef boost::shared_ptr<handler> handler_ptr;
	
	// types
	typedef server<endpoint> type;
	typedef endpoint endpoint_type;
	
	typedef typename endpoint_traits<endpoint>::connection_ptr connection_ptr;
		
	server(boost::asio::io_service& m,handler_ptr h) 
	 : m_ws_endpoint(static_cast< endpoint_type& >(*this)),
	   m_handler(h),
	   m_io_service(m),
	   m_endpoint(),
	   m_acceptor(m)
	{}
	
	void listen(unsigned short port) {
		m_endpoint.port(port);
		m_acceptor.open(m_endpoint.protocol());
		m_acceptor.set_option(boost::asio::socket_base::reuse_address(true));
		m_acceptor.bind(m_endpoint);
		m_acceptor.listen();
		
		this->start_accept();
		
		m_ws_endpoint.alog().at(log::alevel::DEVEL) << "role::server listening on port " << port << log::endl;
		
		m_io_service.run();
	}
protected:
	bool is_server() {
		return true;
	}
	
	handler_ptr get_handler() {
		return m_handler;
	}
private:
	// start_accept creates a new connection and begins an async_accept on it
	void start_accept() {
		connection_ptr con = m_ws_endpoint.create_connection();
		
		m_acceptor.async_accept(
			con->get_raw_socket(),
			boost::bind(
				&type::handle_accept,
				this,
				con,
				boost::asio::placeholders::error
			)
		);
	}
	
	// handle_accept will begin the connection's read/write loop and then reset
	// the server to accept a new connection. Errors returned by async_accept
	// are logged and ingored.
	void handle_accept(connection_ptr con, const boost::system::error_code& error) {
		if (error) {
			m_ws_endpoint.elog().at(log::elevel::ERROR) << "async_accept returned error: " << error << log::endl;
		} else {
			con->start();
		}
		
		this->start_accept();
	}
	
	endpoint_type&					m_ws_endpoint;
	handler_ptr						m_handler;
	boost::asio::io_service&		m_io_service;
	boost::asio::ip::tcp::endpoint	m_endpoint;
	boost::asio::ip::tcp::acceptor	m_acceptor;
};

	
} // namespace role	
} // namespace websocketpp

#endif // WEBSOCKETPP_ROLE_SERVER_HPP
