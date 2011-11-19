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

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>

namespace websocketpp {
	template <typename T>
	struct endpoint_traits;
	
	
	
	class endpoint_base {
	protected:
		boost::asio::io_service	m_io_service;
	};
}

#include "connection.hpp"
#include "sockets/plain.hpp" // should this be here?
#include "logger/logger.hpp"



#include <iostream>
#include <set>

namespace websocketpp {

// endpoint_base provides core functionality used by policy base class constructors


// endpoint host class
template <
	template <class> class role,
	template <class> class socket = socket::plain,
	template <class> class logger = log::logger>
class endpoint 
 : public endpoint_base,
   public role< endpoint<role,socket> >,
   public socket< endpoint<role,socket> > 
{
public:
	typedef endpoint_traits< endpoint<role,socket,logger> > traits;
	
	// get types that we need from our traits class
	typedef typename traits::type type;
	typedef typename traits::handler_ptr handler_ptr;
	typedef typename traits::role_type role_type;
	typedef typename traits::socket_type socket_type;
	typedef typename traits::alogger_type alogger_type;
	typedef typename traits::elogger_type elogger_type;
	typedef typename traits::connection_type connection_type;
	typedef typename traits::connection_ptr connection_ptr;
	
	// Friend is used here to allow the CRTP base classes to access member 
	// functions in the derived endpoint. This is done to limit the use of 
	// public methods in endpoint and its CRTP bases to only those methods 
	// intended for end-application use.
	friend class role< endpoint<role,socket> >;
	friend class socket< endpoint<role,socket> >;
	friend class connection<type,role< type >::template connection,socket< type >::template connection>;
	
	// Highly simplified and preferred C++11 version:
	// friend role_type; 
	// friend socket_type;
	// friend connection_type;
	
	endpoint(handler_ptr h) : role_type(m_io_service,h),socket_type(m_io_service) {}
	
	alogger_type& alog() {
		return m_alog;
	}
	
	elogger_type& elog() {
		return m_elog;
	}
protected:
	connection_ptr create_connection() {
		connection_ptr new_connection(new connection_type(*this));
		m_connections.insert(new_connection);
		
		alog().at(log::alevel::DEVEL) << "Connection created: count is now: " << m_connections.size() << log::endl;
		
		return new_connection;
	}
	
	void remove_connection(connection_ptr con) {
		m_connections.erase(con);
		
		alog().at(log::alevel::DEVEL) << "Connection removed: count is now: " << m_connections.size() << log::endl;
	}
private:
	std::set<connection_ptr>	m_connections;
	alogger_type				m_alog;
	elogger_type				m_elog;
};

// endpoint related types that it and its policy classes need.
template <
	template <class> class role,
	template <class> class socket,
	template <class> class logger>
struct endpoint_traits< endpoint<role, socket, logger> > {
	// type of the endpoint itself
	typedef endpoint<role,socket,logger> type;
	
	// types of the endpoint policies
	typedef role< type > role_type;
	typedef socket< type > socket_type;
	
	typedef logger<log::alevel::value> alogger_type;
	typedef logger<log::elevel::value> elogger_type;
	
	// type of the handler that this endpoint and its connections call back.
	typedef typename role_type::handler handler;
	typedef typename role_type::handler_ptr handler_ptr;
	
	// types of the connections that this endpoint manages and pointers to them
	typedef connection<type,role< type >::template connection,socket< type >::template connection> connection_type;
	typedef boost::shared_ptr<connection_type> connection_ptr;
};

} // namespace websocketpp

#endif // WEBSOCKETPP_ENDPOINT_HPP
