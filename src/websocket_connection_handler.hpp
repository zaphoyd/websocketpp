#ifndef WEBSOCKET_CONNECTION_HANDLER_HPP
#define WEBSOCKET_CONNECTION_HANDLER_HPP

#include <boost/shared_ptr.hpp>

#include <string>
#include <map>

namespace websocketpp {
	class connection_handler;
	typedef boost::shared_ptr<connection_handler> connection_handler_ptr;
}

#include "websocket_session.hpp"

namespace websocketpp {

class connection_handler {
public:
	// validate will be called after a websocket handshake has been received and 
	// before it is accepted. It provides a handler the ability to refuse a 
	// connection based on application specific logic (ex: restrict domains or
	// negotiate subprotocols)
	//
	// resource is the resource requested by the connection
	// headers is a map containing the HTTP headers included in the client 
	//   handshake as key/value strings
	//
	// if validate returns true the connection is allowed, otherwise the 
	//   connection is closed.
	virtual bool validate(session_ptr client) = 0;
	
	// this will be called once the connected websocket is avaliable for 
	// writing messages. client may be a new websocket session or an existing
	// session that was recently passed to this handler.
	virtual void connect(session_ptr client) = 0;
	
	// this will be called when the connected websocket is no longer avaliable
	// for writing messages. This occurs under the following conditions:
	// - Disconnect message recieved from the remote endpoint
	// - Someone (usually this object) calls the disconnect method of session
	// - A disconnect acknowledgement is recieved (in case another object 
	//     calls the disconnect method of session
	// - The connection handler assigned to this client was set to another 
	//     handler
	virtual void disconnect(session_ptr client,const std::string &reason) = 0;
	
	// this will be called when a text message is recieved. Text will be 
	// encoded as UTF-8.
	virtual void message(session_ptr client,const std::string &msg) = 0;
	
	// this will be called when a binary message is recieved. Argument is a 
	// vector of the raw bytes in the message body.
	virtual void message(session_ptr client,
		const std::vector<unsigned char> &data) = 0;
};



}
#endif // WEBSOCKET_CONNECTION_HANDLER_HPP