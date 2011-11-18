/*
build situation
endpoint.hpp
- libboost_system
- libboost_date_time (if you use the default logger)
 
sockets/ssl.hpp
- libcrypto
- libssl

*/

// Application start
#include "endpoint.hpp"
#include "roles/server.hpp"
#include "sockets/ssl.hpp"

typedef websocketpp::endpoint<websocketpp::role::server,websocketpp::socket::ssl> endpoint_type;
//typedef websocketpp::endpoint<websocketpp::role::server> endpoint_type;
typedef websocketpp::role::server<endpoint_type>::handler handler_type;
typedef websocketpp::role::server<endpoint_type>::handler_ptr handler_ptr;

// application headers
class application_server_handler : public handler_type {
	void on_action() {
		std::cout << "application_server_handler::on_action()" << std::endl;
	}
};

/*class application_client_handler : public client_handler {
	void on_action() {
		std::cout << "application_client_handler::on_action()" << std::endl;
	}
};*/


int main () {
	std::cout << "Endpoint 0" << std::endl;
	handler_ptr h(new application_server_handler());
	endpoint_type e(h);
	
	e.alog().set_level(websocketpp::log::alevel::ALL);
	e.elog().set_level(websocketpp::log::elevel::ALL);
	
	
	e.listen(9000);
	//e.connect();
	//e.public_api();
	std::cout << std::endl;

	return 0;
}