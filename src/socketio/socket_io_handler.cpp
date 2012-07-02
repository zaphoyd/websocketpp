/* socket_io_handler.cpp
 * Evan Shimizu, June 2012
 * websocket++ handler implementing the socket.io protocol.
 * https://github.com/LearnBoost/socket.io-spec 
 *
 * This implementation uses the rapidjson library.
 * See examples at https://github.com/kennytm/rapidjson.
 */

#include "socket_io_handler.hpp"

using socketio::socketio_client_handler;
using socketio::socketio_events;
using websocketpp::client;

// Event handlers

void socketio_events::example(const Value& args)
{
   // Event handlers are responsible for knowing what's in args.
   // This function expects a string as the first argument, and doesn't care about the rest.
   std::cout << "Hello! You've just successfully tested this event. Args[0]: " << args[SizeType(0)].GetString() << std::endl;
}


// Websocket++ client handler

void socketio_client_handler::on_fail(connection_ptr con)
{
   std::cout << "Connection failed." << std::endl;
}

void socketio_client_handler::on_open(connection_ptr con)
{
   m_con = con;
   // Create the heartbeat timer and use the same io_service as the main event loop.
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
   // Parse the incoming message according to socket.IO rules
   parse_message(msg->get_payload());
}

// Client Functions
// Note from websocketpp code: methods (except for perform_handshake) will be called
// from outside the io_service.run thread and need to be careful to not touch unsynchronized
// member variables.

std::string socketio_client_handler::perform_handshake(std::string url, std::string socketIoResource)
{
   // Log currently not accessible from this function, outputting to std::cout
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

   reqstream << "POST " << socketIoResource << "/1/ HTTP/1.0\r\n";
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
   std::stringstream iouri;
   iouri << "ws://" << uo.get_host() << ":" << uo.get_port() << socketIoResource << "/1/websocket/" << m_sid;
   m_socketIoUri = iouri.str();
   return m_socketIoUri;
}

void socketio_client_handler::send(const std::string &msg)
{
   if (!m_con)
   {
      std::cerr << "Error: No active session" << std::endl;
      return;
   }

   m_con->alog()->at(websocketpp::log::alevel::DEVEL) << "Sent: " << msg << websocketpp::log::endl;
   m_con->send(msg);
}

void socketio_client_handler::send(unsigned int type, std::string endpoint, std::string msg, unsigned int id)
{
   // Construct the message.
   // Format: [type]:[id]:[endpoint]:[msg]
   std::stringstream package;
   package << type << ":";
   if (id > 0) package << id;
   package << ":" << endpoint << ":" << msg;

   send(package.str());
}

void socketio_client_handler::emit(std::string name, Document& args, std::string endpoint, unsigned int id)
{
   // Add the name to the data being sent.
   Value n;
   n.SetString(name.c_str(), name.length(), args.GetAllocator());
   args.AddMember("name", n, args.GetAllocator());

   // Stringify json
   char writeBuffer[JSON_BUFFER_SIZE];
   memset(writeBuffer, 0, JSON_BUFFER_SIZE);
	FileWriteStream os(stdout, writeBuffer, sizeof(writeBuffer));
	Writer<FileWriteStream> writer(os);
   args.Accept(writer);

   // Extract the message from the stream and format it.
   std::string package(writeBuffer);
   send(5, endpoint, package.substr(0, package.find('\0')), id);
}

void socketio_client_handler::message(std::string msg, std::string endpoint, unsigned int id)
{
   send(3, endpoint, msg, id);
}

void socketio_client_handler::json_message(Document& json, std::string endpoint, unsigned int id)
{
   // Stringify json
   char writeBuffer[JSON_BUFFER_SIZE];
   memset(writeBuffer, 0, JSON_BUFFER_SIZE);
	FileWriteStream os(stdout, writeBuffer, sizeof(writeBuffer));
	Writer<FileWriteStream> writer(os);
   json.Accept(writer);

   // Extract the message from the stream and format it.
   std::string package(writeBuffer);
   send(4, endpoint, package.substr(0, package.find('\0')), id);
}

bool socketio_client_handler::bind_event(std::string name, eventFunc func)
{
   // If the name is already in use, don't replace it.
   if (m_events.count(name) == 0)
   {
      m_events[name] = func;
      return true;
   }

   return false;
}

bool socketio_client_handler::unbind_event(std::string name)
{
   // Delete from map if the key exists.
   if (m_events.count(name) > 0)
   {
      m_events.erase(m_events.find(name));
      return true;
   }
   return false;
}

void socketio_client_handler::close()
{
   if (!m_con)
   {
      std::cerr << "Error: No active session" << std::endl;
      return;
   }

   send(3, "disconnect", "");
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
   if (m_con)
   {
      m_con->send("2::");
      std::cout << "Sent Heartbeat" << std::endl;
   }
}

void socketio_client_handler::heartbeat()
{
   send_heartbeat();

   m_heartbeatTimer->expires_at(m_heartbeatTimer->expires_at() + boost::posix_time::seconds(m_heartbeatTimeout));
   m_heartbeatTimer->async_wait(boost::bind(&socketio_client_handler::heartbeat, this));
}

void socketio_client_handler::parse_message(const std::string &msg)
{
   // Parse response according to socket.IO rules.
   // https://github.com/LearnBoost/socket.io-spec

   boost::cmatch matches;
   const boost::regex expression("([0-8]):([0-9]*):([^:]*)[:]?(.*)");

   if(boost::regex_match(msg.c_str(), matches, expression))
   {
      int type;
      int msgId;
      Document json;

      // Attempt to parse the first match as an int.
      std::stringstream convert(matches[1]);
      if (!(convert >> type)) type = -1;

      // Store second param for parsing as message id. Not every type has this, so if it's missing we just use 0 as the ID.
      std::stringstream convertId(matches[2]);
      if (!(convertId >> msgId)) msgId = 0;

      switch (type)
      {
      // Disconnect
      case (0):
         m_con->alog()->at(websocketpp::log::alevel::DEVEL) << "Received message type 0 (Disconnect)" << websocketpp::log::endl;
         close();
         break;
      // Connection Acknowledgement
      case (1):
         m_con->alog()->at(websocketpp::log::alevel::DEVEL) << "Received Message type 1 (Connect): " << msg << websocketpp::log::endl;
         break;
      // Heartbeat
      case (2):
         m_con->alog()->at(websocketpp::log::alevel::DEVEL) << "Received Message type 2 (Heartbeat)" << websocketpp::log::endl;
         send_heartbeat();
         break;
      // Message
      case (3):
         m_con->alog()->at(websocketpp::log::alevel::DEVEL) << "Received Message type 3 (Message): " << msg << websocketpp::log::endl;
         on_socketio_message(msgId, matches[3], matches[4]);
         break;
      // JSON Message
      case (4):
         m_con->alog()->at(websocketpp::log::alevel::DEVEL) << "Received Message type 4 (JSON Message): " << msg << websocketpp::log::endl;
         // Parse JSON
         if (json.Parse<0>(matches[4].str().c_str()).HasParseError())
         {
            m_con->elog()->at(websocketpp::log::elevel::WARN) << "JSON Parse Error: " << matches[4] << websocketpp::log::endl;
            return;
         }
         on_socketio_json(msgId, matches[3], json);
         break;
      // Event
      case (5):
         m_con->alog()->at(websocketpp::log::alevel::DEVEL) << "Received Message type 5 (Event): " << msg << websocketpp::log::endl;
         // Parse JSON
         if (json.Parse<0>(matches[4].str().c_str()).HasParseError())
         {
            m_con->elog()->at(websocketpp::log::elevel::WARN) << "JSON Parse Error: " << matches[4] << websocketpp::log::endl;
            return;
         }
         if (!json["name"].IsString())
         {
            m_con->elog()->at(websocketpp::log::elevel::WARN) << "Invalid socket.IO Event" << websocketpp::log::endl;
            return;
         }
         on_socketio_event(msgId, matches[3], json["name"].GetString(), json["args"]);
         break;
      // Ack
      case (6):
         m_con->alog()->at(websocketpp::log::alevel::DEVEL) << "Received Message type 6 (ACK)" << websocketpp::log::endl;
         on_socketio_ack(matches[4]);
         break;
      // Error
      case (7):
         m_con->alog()->at(websocketpp::log::alevel::DEVEL) << "Received Message type 7 (Error): " << msg << websocketpp::log::endl;
         on_socketio_error(matches[3], matches[4].str().substr(0, matches[4].str().find("+")), matches[4].str().substr(matches[4].str().find("+")+1));
         break;
      // Noop
      case (8):
         m_con->alog()->at(websocketpp::log::alevel::DEVEL) << "Received Message type 8 (Noop)" << websocketpp::log::endl;
         break;
      default:
         std::cout << "Invalid Socket.IO message type: " << type << std::endl;
         break;
      }
   }
   else
   {
      std::cout << "Non-Socket.IO message: " << msg << std::endl;
   }
}

// This is where you'd add in behavior to handle the message data for your own app.
void socketio_client_handler::on_socketio_message(int msgId, std::string msgEndpoint, std::string data)
{
   std::cout << "Received message (" << msgId << ") " << data << std::endl;
}

// This is where you'd add in behavior to handle json messages.
void socketio_client_handler::on_socketio_json(int msgId, std::string msgEndpoint, Document& json)
{
   std::cout << "Received JSON Data (" << msgId << ")" << std::endl;
}

// This is where you'd add in behavior to handle events.
// By default, nothing is done with the endpoint or ID params.
void socketio_client_handler::on_socketio_event(int msgId, std::string msgEndpoint, std::string name, const Value& args)
{
   std::cout << "Received event (" << msgId << ") " << std::endl;

   if (m_events.count(name) > 0)
   {
      eventFunc func = m_events[name];
      ((*m_socketioEvents).*(func))(args);
   }
   else std::cout << "No bound event with name: " << name << std::endl;
}

// This is where you'd add in behavior to handle ack
void socketio_client_handler::on_socketio_ack(std::string data)
{
   std::cout << "Received ACK: " << data << std::endl;
}

// This is where you'd add in behavior to handle errors
void socketio_client_handler::on_socketio_error(std::string endpoint, std::string reason, std::string advice)
{
   std::cout << "Received Error: " << reason << " Advice: " << advice << std::endl;
}