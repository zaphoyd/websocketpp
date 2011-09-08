#include "websocket_server.hpp"

#include <boost/bind.hpp>

#include <iostream>

using websocketpp::server;

server::server(boost::asio::io_service& io_service, 
			   const tcp::endpoint& endpoint,
			   const std::string& host,
			   connection_handler_ptr defc)
	: m_host(host),
	  m_io_service(io_service), 
	  m_acceptor(io_service, endpoint), 
	  m_def_con_handler(defc) {
	this->start_accept();
}

void server::start_accept() {
	session_ptr new_ws(new session(m_io_service,m_host,m_def_con_handler));
				
	m_acceptor.async_accept(
		new_ws->socket(),
		boost::bind(
			&server::handle_accept,
			this,
			new_ws,
			boost::asio::placeholders::error
		)
	);
}

void server::handle_accept(session_ptr session,
	const boost::system::error_code& error) {
	
	if (!error) {
		session->start();
	} else {
		std::cout << "Error" << std::endl;
	}
	
	this->start_accept();
}