/* socket_io_handler.hpp
 * Evan Shimizu, June 2012
 * websocket++ handler implementing the socket.io protocol.
 * https://github.com/LearnBoost/socket.io-spec 
 *
 * This implementation uses the rapidjson library.
 * See examples at https://github.com/kennytm/rapidjson.
 */

#ifndef SOCKET_IO_HANDLER_HPP
#define SOCKET_IO_HANDLER_HPP

// C4355: this used in base member initializer list
#pragma warning(disable:4355)

#include <boost/shared_ptr.hpp>
#include <boost/regex.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "rapidjson/document.h"		// rapidjson's DOM-style API
#include "rapidjson/prettywriter.h"	// for stringify JSON
#include "rapidjson/filestream.h"	// wrapper of C stream for prettywriter as output
#include "rapidjson/filewritestream.h"

#include "../roles/client.hpp"
#include "../websocketpp.hpp"

#include <map>
#include <string>
#include <queue>

#define JSON_BUFFER_SIZE 20000

using websocketpp::client;
using namespace rapidjson;

namespace socketio {

// Class for event callbacks.
// Class is automatically created  on the stack to handle calling the function when an event callback is triggered.
// Class is broken out from the main handler class to allow for easier editing of functions
// and modularity of code.
class socketio_events
{
public:
   void example(const Value& args);
};

class socketio_client_handler : public client::handler {
public:
   socketio_client_handler() :
      m_heartbeatActive(false)
   {
      // You can bind events inside or outside of the constructor.
      bind_event("anevent", &socketio_events::example);
   }

   virtual ~socketio_client_handler() {}

   // Callbacks
   void on_fail(connection_ptr con);

   void on_open(connection_ptr con);

   void on_close(connection_ptr con);

   void on_message(connection_ptr con, message_ptr msg);

   // Client Functions - such as send, etc.

   // Function pointer to a event handler.
   // Args is an array, managed by rapidjson, and could be null
   typedef std::function<void (socketio_events&, const Value&)> eventFunc;

   // Performs a socket.IO handshake
   // https://github.com/LearnBoost/socket.io-spec
   // param - url takes a ws:// address with port number
   // param - socketIoResource is the resource where the server is listening. Defaults to "/socket.io".
   // Returns a socket.IO url for performing the actual connection.
   std::string perform_handshake(std::string url, std::string socketIoResource = "/socket.io");

   // Sends a plain string to the endpoint. No special formatting performed to the string.
   void send(const std::string &msg);

   // Allows user to send a custom socket.IO message
   void send(unsigned int type, std::string endpoint, std::string msg, unsigned int id = 0);

   // Emulates the emit function from socketIO (type 5) 
   void emit(std::string name, Document& args, std::string endpoint = "", unsigned int id = 0);

   // Sends a plain message (type 3)
   void message(std::string msg, std::string endpoint = "", unsigned int id = 0);

   // Sends a JSON message (type 4)
   void json_message(Document& json, std::string endpoint = "", unsigned int id = 0);

   // Binds a function to a name. Function will be passed a a Value ref as the only argument.
   // If the function already exists, this function returns false. You must call unbind_event
   // on the name of the function first to re-bind a name.
   bool bind_event(std::string name, eventFunc func);

   // Removes the binding between event [name] and the function associated with it.
   bool unbind_event(std::string name);

   // Closes the connection
   void close();

   // Heartbeat operations.
   void start_heartbeat();
   void stop_heartbeat();

private:
   // Sends a heartbeat to the server.
   void send_heartbeat();

   // Called when the heartbeat timer fires.
   void heartbeat();

   // Parses a socket.IO message received
   void parse_message(const std::string &msg);

   // Message Parsing callbacks.
   void on_socketio_message(int msgId, std::string msgEndpoint, std::string data);
   void on_socketio_json(int msgId, std::string msgEndpoint, Document& json);
   void on_socketio_event(int msgId, std::string msgEndpoint, std::string name, const Value& args);
   void on_socketio_ack(std::string data);
   void on_socketio_error(std::string endppoint, std::string reason, std::string advice);

   // Connection pointer for client functions.
   connection_ptr m_con;

   // Socket.IO server settings
   std::string m_sid;
   unsigned int m_heartbeatTimeout;
   unsigned int m_disconnectTimeout;
   std::string m_socketIoUri;

   // Currently we assume websocket as the transport, though you can find others in this string
   std::string m_transports;

   // Heartbeat variabes.
   std::unique_ptr<boost::asio::deadline_timer> m_heartbeatTimer;
   bool m_heartbeatActive;

   // Event bindings
   std::map<std::string, eventFunc> m_events;
};

typedef boost::shared_ptr<socketio_client_handler> socketio_client_handler_ptr;

}

#endif // SOCKET_IO_HANDLER_HPP