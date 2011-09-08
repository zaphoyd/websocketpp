#ifndef WEBSOCKET_SERVER_HPP
#define WEBSOCKET_SERVER_HPP

#include "websocket_session.hpp"

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>

using boost::asio::ip::tcp;

namespace websocketpp {

class server {
	public:
		server(boost::asio::io_service& io_service, 
			   const tcp::endpoint& endpoint,
			   const std::string& host,
			   connection_handler_ptr defc);
		
		// creates a new session object and connects the next websocket
		// connection to it.
		void start_accept();
		
		// if no errors starts the session's read loop and returns to the
		// start_accept phase.
		void handle_accept(session_ptr session,
			const boost::system::error_code& error);
	
	private:
		std::string 				m_host;
		boost::asio::io_service&	m_io_service;
		tcp::acceptor				m_acceptor;
		connection_handler_ptr		m_def_con_handler;
};

typedef boost::shared_ptr<server> server_ptr;

}

#endif // WEBSOCKET_SERVER_HPP