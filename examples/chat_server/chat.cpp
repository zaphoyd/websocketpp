#include "chat.hpp"

using websocketchat::chat_handler;


bool chat_handler::validate(websocketpp::session_ptr client) {
	// We only know about the chat resource
	if (client->get_request() != "/chat") {
		client->set_http_error(404);
		return false;
	}
	
	// Require specific origin example
	if (client->get_header("Sec-WebSocket-Origin") != "http://zaphoyd.com") {
		client->set_http_error(403);
		return false;
	}
	
	return true;
}


void connect(session_ptr client) {
	std::cout << "client " << client << " joined the lobby." << std::endl;
	m_connections.insert(client);

	// send user list to all clients
	// send signon message from server to all other clients.

}
