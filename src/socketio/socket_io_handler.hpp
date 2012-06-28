/* Attempt to define a socket.IO handler for websocketpp */

#ifndef SOCKET_IO_HANDLER_HPP
#define SOCKET_IO_HANDLER_HPP

#include <boost/shared_ptr.hpp>
#include <boost/regex.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "../roles/client.hpp"
#include "../websocketpp.hpp"

#include <map>
#include <string>
#include <queue>

using websocketpp::client;

namespace socketio {

class socketio_client_handler : public client::handler {
public:
   // Constructor for initializing timer.
   socketio_client_handler() :
      m_heartbeatActive(false)
   {
      m_heartbeatTimer = std::unique_ptr<boost::asio::deadline_timer>(new boost::asio::deadline_timer(m_heartbeatService, boost::posix_time::seconds(0)));
   }

   virtual ~socketio_client_handler() {}

   void on_fail(connection_ptr con);

   void on_open(connection_ptr con);

   void on_close(connection_ptr con);

   void on_message(connection_ptr con, message_ptr msg);

   // Client Functions - such as send, etc.

   // Performs a socket.IO handshake.
   // Returns a socket.IO url for performing the actual connection.
   std::string perform_handshake(const std::string &url);

   // Sends a plain string to the endpoint. No special formatting performed to the string.
   void send(const std::string &msg);

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

   // Connection pointer for client functions.
   connection_ptr m_con;

   // Socket.IO server settings
   std::string m_sid;
   unsigned int m_heartbeatTimeout;
   unsigned int m_disconnectTimeout;

   // Currently we assume websocket as the transport, though you can find others in this string
   std::string m_transports;

   // Heartbeat variabes.
   boost::asio::io_service m_heartbeatService;
   std::unique_ptr<boost::asio::deadline_timer> m_heartbeatTimer;
   bool m_heartbeatActive;
};

typedef boost::shared_ptr<socketio_client_handler> socketio_client_handler_ptr;

}

#endif // SOCKET_IO_HANDLER_HPP