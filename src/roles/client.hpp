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

#ifndef WEBSOCKETPP_ROLE_CLIENT_HPP
#define WEBSOCKETPP_ROLE_CLIENT_HPP

#include "../endpoint.hpp"
#include "../uri.hpp"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>

#include <iostream>

using boost::asio::ip::tcp;

namespace websocketpp {
namespace role {

template <class endpoint>
class client {
public:
	// Connection specific details
	template <typename connection_type>
	class connection {
	public:
		typedef connection<connection_type> type;
		typedef endpoint endpoint_type;
	protected:
		 connection(endpoint& e) : m_endpoint(e) {}
		
		void async_init() {
			write_request();
		}
		
		void write_request() {
			// async write to handle_write
		}
		
		void handle_write_request(const boost::system::error_code& error) {
			if (error) {
				m_endpoint.elog().at(log::elevel::ERROR) << "Error writing WebSocket request. code: " << error << log::endl;
				m_connection.terminate(false);
				return;

			}
			
			read_request();
		}
		
		void read_request() {
			boost::asio::async_read_until(
				m_connection.get_socket(),
				m_connection.buffer(),
					"\r\n\r\n",
				boost::bind(
					&type::handle_read_request,
					m_connection.shared_from_this(),
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred
				)
			);
		}
		
		// 
		void handle_read_request(const boost::system::error_code& error,
								 std::size_t bytes_transferred)
		{
			if (error) {
				m_endpoint.elog().at(log::elevel::ERROR) << "Error reading HTTP request. code: " << error << log::endl;
				m_connection.terminate(false);
				return;
			}
			
			// read 
			
			
			// start session loop
		}
	private:
		endpoint&           m_endpoint;
        connection_type&    m_connection;
	};
    
	// types
	typedef client<endpoint> type;
	typedef endpoint endpoint_type;
	
	typedef typename endpoint_traits<endpoint>::connection_ptr connection_ptr;
	typedef typename endpoint_traits<endpoint>::handler_ptr handler_ptr;
	
    // handler interface callback class
	class handler_interface {
	public:
		// Required
		virtual void on_open(connection_ptr connection) {};
		virtual void on_close(connection_ptr connection) {};
		virtual void on_fail(connection_ptr connection) {}
		
		virtual void on_message(connection_ptr connection,message::data_ptr) {};
		
		// Optional
		virtual bool on_ping(connection_ptr connection,std::string) {return true;}
		virtual void on_pong(connection_ptr connection,std::string) {}
		
	};
	
	client (boost::asio::io_service& m) 
	 : m_state(UNINITIALIZED),
	   m_endpoint(static_cast< endpoint_type& >(*this)),
	   m_io_service(m),
	   m_resolver(m) {}
	
	connection_ptr connect(const std::string& u) {
		// TODO: will throw, should we catch and wrap?
		uri location(u);
		
		if (location.get_secure() && !m_endpoint.is_secure()) {
			// TODO: what kind of exception does client throw?
			throw "";
		}
		
		tcp::resolver::query query(location.get_host(),location.get_port_str());
		tcp::resolver::iterator iterator = m_resolver.resolve(query);
		
		connection_ptr con = m_endpoint.create_connection();
		
		boost::asio::async_connect(
			con->get_raw_socket(),
			iterator,
			boost::bind(
				&endpoint_type::handle_connect,
				endpoint_type::shared_from_this(),
				con,
				boost::asio::placeholders::error
			)
		); 
		m_state = CONNECTING;
		
		return con;
	}
	
	// TODO: add a `perpetual` option
	void run() {
		m_io_service.run();
	}
	
	void reset() {
		m_io_service.reset();
	}
	
protected:
	bool is_server() {
		return false;
	}
private:
	enum state {
		UNINITIALIZED = 0,
		INITIALIZED = 1,
		CONNECTING = 2,
		CONNECTED = 3
	};
	
	void handle_connect(connection_ptr con, const boost::system::error_code& error) {
		if (!error) {
			m_endpoint.alog().at(log::alevel::CONNECT) << "Successful connection" << log::endl;
			
			m_state = CONNECTED;
			con->start();
		} else {
			m_endpoint.elog().at(log::elevel::ERROR) << "An error occurred while establishing a connection: " << error << log::endl;
			
			// TODO: fix
			throw "client error";
		}
	}
	
	state						m_state;
	endpoint_type&				m_endpoint;
	boost::asio::io_service&	m_io_service;
	tcp::resolver				m_resolver;
};


} // namespace role
} // namespace websocketpp

#endif // WEBSOCKETPP_ROLE_CLIENT_HPP
