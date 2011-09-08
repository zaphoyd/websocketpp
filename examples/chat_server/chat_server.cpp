#include "chat.hpp"

#include <websocketpp/websocket_server.hpp>
#include <boost/asio.hpp>

#include <iostream>

using boost::asio::ip::tcp;

int main(int argc, char* argv[]) {
	std::string host = "localhost:5000";
	short port = 5000;
	
	if (argc == 3) {
		port = atoi(argv[2]);
		
		std::stringstream temp;
		temp << argv[1] << ":" << port;
		
		host = temp.str();
	}
	
	websocketchat::chat_handler_ptr chat_handler(new websocketchat::chat_handler());
	
	try {
		boost::asio::io_service io_service;
		tcp::endpoint endpoint(tcp::v6(), port);
		
		websocketpp::server_ptr server(
			new websocketpp::server(io_service,endpoint,host,chat_handler)
		);
		
		std::cout << "Starting chat server on " << host << std::endl;
		
		io_service.run();
	} catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
	}
	
	return 0;
}
