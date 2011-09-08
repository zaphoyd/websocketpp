#include "echo.hpp"

#include "../../src/websocketpp.hpp"
#include <boost/asio.hpp>

#include <iostream>

using boost::asio::ip::tcp;

int main(int argc, char* argv[]) {
	std::string host = "localhost:5000";
	short port = 5000;
	
	if (argc == 3) {
		// TODO: input validation?
		port = atoi(argv[2]);

		
		std::stringstream temp;
		temp << argv[1] << ":" << port;
		
		host = temp.str();
	}
	
	websocketecho::echo_handler_ptr echo_handler(new websocketecho::echo_handler());
	
	try {
		boost::asio::io_service io_service;
		tcp::endpoint endpoint(tcp::v6(), port);
		
		websocketpp::server_ptr server(
			new websocketpp::server(io_service,endpoint,echo_handler)
		);
		
		server->add_host(host);
		
		std::cout << "Starting echo server on " << host << std::endl;
		
		io_service.run();
	} catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
	}
	
	return 0;
}
