#include "echo.hpp"

using websocketecho::echo_handler;

bool echo_handler::validate(websocketpp::session_ptr client) {
	return true;
}

void echo_handler::message(websocketpp::session_ptr client, const std::string &msg) {
	client->send(msg);
}

void echo_handler::message(websocketpp::session_ptr client,
	const std::vector<unsigned char> &data) {
	client->send(data);
}
