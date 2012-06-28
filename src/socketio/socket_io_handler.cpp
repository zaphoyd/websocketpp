#include "socket_io_handler.hpp"

using socketio::socketio_client_handler;
using websocketpp::client;

void socketio_client_handler::on_fail(connection_ptr con)
{
   std::cout << "Connection failed." << std::endl;
}

void socketio_client_handler::on_open(connection_ptr con)
{
   m_con = con;
   // Reassign heartbeat timer to actual io_service being used.
   m_heartbeatTimer = std::unique_ptr<boost::asio::deadline_timer>(new boost::asio::deadline_timer(con->get_io_service(), boost::posix_time::seconds(0)));
   start_heartbeat();

   std::cout << "Connected." << std::endl;
}

void socketio_client_handler::on_close(connection_ptr con)
{
   m_con = connection_ptr();
   stop_heartbeat();

   std::cout << "Client Disconnected." << std::endl;
}

void socketio_client_handler::on_message(connection_ptr con, message_ptr msg)
{
   // For now just drop message in log until parser is online
   std::cout << msg->get_payload() << std::endl;
}

// Client Functions
// Note from websocketpp code: methods (except for perform_handshake) will be called
// from outside the io_service.run thread and need to be careful to not touch unsynchronized
// member variables.

// Performs a socket.IO handshake
// https://github.com/LearnBoost/socket.io-spec
std::string socketio_client_handler::perform_handshake(const std::string &url)
{
   std::cout << "Parsing websocket uri..." << std::endl;
   websocketpp::uri uo(url);

   // Declare boost io_service
   boost::asio::io_service io_service;

   std::cout << "Connecting to Server..." << std::endl;

   // Resolve query
   tcp::resolver r(io_service);
   tcp::resolver::query q(uo.get_host(), uo.get_port_str());
   tcp::socket socket(io_service);
   boost::asio::connect(socket, r.resolve(q));

   // Form initial post request.
   boost::asio::streambuf request;
   std::ostream reqstream(&request);

   reqstream << "POST /socket.io/1/ HTTP/1.0\r\n";
   reqstream << "Host: " << uo.get_host() << "\r\n";
   reqstream << "Accept: */*\r\n";
   reqstream << "Connection: close\r\n\r\n";

   std::cout << "Sending Handshake Post Request..." << std::endl;

   // Write request.
   boost::asio::write(socket, request);

   // Receive response
   boost::asio::streambuf response;
   boost::asio::read_until(socket, response, "\r\n");

   // Parse response
   std::istream resp_stream(&response);

   // Extract HTTP version, status, and message.
   std::string httpver;
   unsigned int status;
   std::string status_msg;

   resp_stream >> httpver >> status;
   std::getline(resp_stream, status_msg);

   // Log response
   std::cout << "Received Response:" << std::endl;
   std::cout << httpver << " " << status << std::endl;
   std::cout << status_msg << std::endl;

   // Read response headers. Terminated with double newline.
   boost::asio::read_until(socket, response, "\r\n\r\n");

   // Log headers.
   std::string header;

   while (std::getline(resp_stream, header) && header[0] != '\r')
   {
      std::cout << header << std::endl;
   }

   // Handle errors
   if (!resp_stream || httpver.substr(0, 5) != "HTTP/")
   {
      std::cerr << "Invalid HTTP protocol: " << httpver << std::endl;
      throw websocketpp::exception("Socket.IO Handshake: Invalid HTTP protocol: " + httpver, websocketpp::error::GENERIC);
   }
   switch (status)
   {
   case(200):
      std::cout << "Server accepted connection." << std::endl;
      break;
   case(401):
   case(503):
      std::cerr << "Server rejected client connection" << std::endl;
      throw websocketpp::exception("Socket.IO Handshake: Server rejected connection with code " + status, websocketpp::error::GENERIC);
      break;
   default:
      std::cerr << "Server returned unknown status code: " << status << std::endl;
      throw websocketpp::exception("Socket.IO Handshake: Server responded with unknown code " + status, websocketpp::error::GENERIC);
      break;
   }

   // Get the body components.
   std::string body;

   std::getline(resp_stream, body, '\0');
   boost::cmatch matches;
   const boost::regex expression("([0-9|a-f]*):([0-9]*):([0-9]*):(.*)");

   if (boost::regex_match(body.c_str(), matches, expression))
   {
      m_sid = matches[1];
      
      m_heartbeatTimeout = atoi(matches[2].str().c_str());
      if (m_heartbeatTimeout <= 0) m_heartbeatTimeout = 0;
      
      m_disconnectTimeout = atoi(matches[3].str().c_str());
      
      m_transports = matches[4];
      if (m_transports.find("websocket") == std::string::npos)
      {
         std::cerr << "Server does not support websocket transport: " << m_transports << std::endl;
         websocketpp::exception("Socket.IO Handshake: Server does not support websocket transport", websocketpp::error::GENERIC);
      }
   }

   // Log socket.IO info
   std::cout << std::endl << "Session ID: " << m_sid << std::endl;
   std::cout << "Heartbeat Timeout: " << m_heartbeatTimeout << std::endl;
   std::cout << "Disconnect Timeout: " << m_disconnectTimeout << std::endl;
   std::cout << "Allowed Transports: " << m_transports << std::endl;

   // Form the complete connection uri. Default transport method is websocket (since we are websocketpp).
   // If secure websocket connection is desired, replace ws with wss.
   return "ws://" + uo.get_host() + "/socket.io/1/websocket/" + m_sid;
}

void socketio_client_handler::send(const std::string &msg)
{
   if (!m_con)
   {
      std::cerr << "Error: No active session" << std::endl;
      return;
   }

   m_con->send(msg);
}

void socketio_client_handler::close()
{
   if (!m_con)
   {
      std::cerr << "Error: No active session" << std::endl;
      return;
   }

   m_con->close(websocketpp::close::status::GOING_AWAY, "");
}

void socketio_client_handler::start_heartbeat()
{
   // Heartbeat is already active so don't do anything.
   if (m_heartbeatActive) return;
   
   // Check valid heartbeat wait time.
   if (m_heartbeatTimeout > 0)
   {
      m_heartbeatTimer->expires_at(m_heartbeatTimer->expires_at() + boost::posix_time::seconds(m_heartbeatTimeout));
      m_heartbeatActive = true;
      m_heartbeatTimer->async_wait(boost::bind(&socketio_client_handler::heartbeat, this));

      std::cout << "Sending heartbeats. Timeout: " << m_heartbeatTimeout << std::endl;
   }
}

void socketio_client_handler::stop_heartbeat()
{
   // Timer is already stopped.
   if (!m_heartbeatActive) return;

   // Stop the heartbeats.
   m_heartbeatActive = false;
   m_heartbeatTimer->cancel();

   std::cout << "Stopped sending heartbeats." << std::endl;
}

void socketio_client_handler::send_heartbeat()
{
   m_con->send("2::");

   std::cout << "Sent Heartbeat" << std::endl;
}

void socketio_client_handler::heartbeat()
{
   send_heartbeat();

   m_heartbeatTimer->expires_at(m_heartbeatTimer->expires_at() + boost::posix_time::seconds(m_heartbeatTimeout));
   m_heartbeatTimer->async_wait(boost::bind(&socketio_client_handler::heartbeat, this));
}

void socketio_client_handler::parse_message(const std::string &msg)
{
   // Parse response here.
   // https://github.com/LearnBoost/socket.io-spec
}