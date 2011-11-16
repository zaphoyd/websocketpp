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

#include <boost/shared_ptr.hpp>

#include <iostream>

namespace websocketpp {


template <typename T>
struct endpoint_traits;

// endpoint_base provides core functionality used by policy base class constructors
class endpoint_base {
protected:
	boost::asio::io_service	m_io_service;
};

// endpoint host class
template <template <class> class role,template <class> class socket>
class endpoint 
 : public endpoint_base,
   public role< endpoint<role,socket> >,
   public socket< endpoint<role,socket> > 
{
public:
	typedef endpoint_traits< endpoint<role,socket> > traits;
	
	// get types that we need from our traits class
	typedef typename traits::handler_ptr handler_ptr;
	typedef typename traits::role_type role_type;
	typedef typename traits::socket_type socket_type;
	typedef typename traits::connection_type connection_type;
	typedef typename traits::connection_ptr connection_ptr;
	
	endpoint(handler_ptr h) : role_type(m_io_service,h),socket_type(m_io_service) {
		std::cout << "Setup endpoint" << std::endl;
	}
	
	void start() {
		std::cout << "Connect" << std::endl;
		
		connection_ptr con = create_connection();
		con->security_handshake();
	}
	
	connection_ptr create_connection() {
		connection_ptr new_connection(new connection_type(*this));
		m_connections.insert(new_connection);
		return new_connection;
	}
	
	void remove_connection(connection_ptr con) {
		m_connections.erase(con);
	}
private:
	std::set<connection_ptr> m_connections;
};

// endpoint related types that it and its policy classes need.
template <template <class> class role,template <class> class socket>
struct endpoint_traits< endpoint<role, socket> > {
	// type of the endpoint itself
	typedef endpoint<role,socket> type;
	
	// types of the endpoint policies
	typedef role< type > role_type;
	typedef socket< type > socket_type;
	
	// type of the handler that this endpoint and its connections call back.
	typedef typename role_type::handler handler;
	typedef typename role_type::handler_ptr handler_ptr;
	
	// types of the connections that this endpoint manages and pointers to them
	typedef connection<type,socket< type >::template connection> connection_type;
	typedef boost::shared_ptr<connection_type> connection_ptr;
};

} // namespace websocketpp

#endif // WEBSOCKETPP_ENDPOINT_HPP
