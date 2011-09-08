#ifndef ECHO_HANDLER_HPP
#define ECHO_HANDLER_HPP

#include "../../src/websocket_connection_handler.hpp"
#include <boost/shared_ptr.hpp>

#include <string>
#include <vector>

namespace websocketecho {

class echo_handler : public websocketpp::connection_handler {
public:
	echo_handler() {}
	virtual ~echo_handler() {}
	
	// The echo server allows all domains is protocol free.
	bool validate(websocketpp::session_ptr client);
	
	// an echo server is stateless. The handler has no need to keep track of connected
	// clients.
	void connect(websocketpp::session_ptr client) {}
	void disconnect(websocketpp::session_ptr client,const std::string &reason) {}
	
	// both text and binary messages are echoed back to the sending client.
	void message(websocketpp::session_ptr client,const std::string &msg);
	void message(websocketpp::session_ptr client,
		const std::vector<unsigned char> &data);
};

typedef boost::shared_ptr<echo_handler> echo_handler_ptr;

}

#endif // ECHO_HANDLER_HPP
