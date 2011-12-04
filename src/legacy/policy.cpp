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

typedef websocketpp::endpoint<websocketpp::role::server,websocketpp::socket::plain> endpoint_type;
//typedef websocketpp::endpoint<websocketpp::role::server> endpoint_type;
typedef websocketpp::role::server<endpoint_type>::handler handler_type;
typedef websocketpp::role::server<endpoint_type>::handler_ptr handler_ptr;

// application headers
class application_server_handler : public handler_type {
public:
	void validate(handler_type::connection_ptr connection) {
		//std::cout << "state: " << connection->get_state() << std::endl;
	}
	
	void on_open(handler_type::connection_ptr connection) {
		//std::cout << "connection opened" << std::endl;
	}
	
	void on_close(handler_type::connection_ptr connection) {
		//std::cout << "connection closed" << std::endl;
	}
	
	void on_message(connection_ptr connection,websocketpp::utf8_string_ptr msg) {
		//std::cout << "got message: " << *msg << std::endl;
		connection->send(*msg);
	}
	void on_message(connection_ptr connection,websocketpp::binary_string_ptr data) {
		//std::cout << "got binary message of length: " << data->size() << std::endl;
		connection->send(*data);
	}
	
	void http(handler_type::connection_ptr connection) {
		connection->set_body("HTTP Response!!");
	}
	
	void on_fail(handler_type::connection_ptr connection) {
		std::cout << "connection failed" << std::endl;
	}
};

typedef websocketpp::endpoint<websocketpp::role::client,websocketpp::socket::plain> basic_client;
typedef websocketpp::role::client<basic_client>::handler client_handler_type;
typedef websocketpp::role::client<basic_client>::handler_ptr client_handler_ptr;

class application_client_handler : public client_handler_type {
	void validate(handler_type::connection_ptr connection) {
		std::cout << "state: " << connection->get_state() << std::endl;
	}
	
	void on_open(handler_type::connection_ptr connection) {
		std::cout << "connection opened" << std::endl;
	}
	
	void on_close(handler_type::connection_ptr connection) {
		std::cout << "connection closed" << std::endl;
	}
	
	void on_message(connection_ptr connection,websocketpp::utf8_string_ptr msg) {
		std::cout << "got message: " << *msg << std::endl;
		connection->send(*msg);
	}
	void on_message(connection_ptr connection,websocketpp::binary_string_ptr data) {
		std::cout << "got binary message of length: " << data->size() << std::endl;
		connection->send(*data);
	}
	
	void http(handler_type::connection_ptr connection) {
		connection->set_body("HTTP Response!!");
	}
	
	void on_fail(handler_type::connection_ptr connection) {
		std::cout << "connection failed" << std::endl;
	}
};


int main () {
	handler_ptr h(new application_client_handler());
	endpoint_type e(h);
	
	e.alog().set_level(websocketpp::log::alevel::ALL);
	e.elog().set_level(websocketpp::log::elevel::ALL);
	
	
	e.connect("ws://localhost:9002");

	return 0;
}