How to use this library

WebSocket++ is a C++ websocket server library implemented using the Boost Asio networking stack. It is designed to provide a simple interface.

Built on Asio's proactor asynchronous event loop. 

Building a program using WebSocket++ has two parts
1. Implement a connection handler.
This is done by subclassing websocketpp::connection_handler. Each websocket connection is attached to a connection handler. 
The handler implements the following methods:

validate:
Called after the client handshake is received but before the connection is accepted. Allows cookie authentication, origin checking, subprotocol negotiation, etc.

connect:
Called when the connection has been established and writes are allowed.

disconnect:
Called when the connection has been disconnected.

message: text and binary variants
Called when a new websocket message is received.

The handler has access to the following websocket session API:
get_header
returns the value of an HTTP header sent by the client during the handshake.

get_request
returns the resource requested by the client in the handshake.

set_handler:
pass responsibility for this connection to another connection handler.

set_http_error:
reject the connection with a specific HTTP error.

add_header
adds an HTTP header to the server handshake.

set_subprotocol
selects a subprotocol for the connection.

send: text and binary variants
send a websocket message.

ping:
send a ping.

2. Start Asio's event loop with a TCP endpoint and your connection handler

There are two example programs in the examples directory that demonstrate this use pattern. 
One is a trivial stateless echo server, the other is a simple web based chat client. 
Both include example javascript clients. The echo server is suitable for use with automated 
testing suites such as the Autobahn test suite.

By default, a single connection handler object is used for all connections. 
If needs require, that default handler can either store per-connection state itself 
or create new handlers and pass off responsibility for the connection to them.

How to build this library

Build static library
make

Build and install in system include directories
make install

Available flags:
- SHARED=1: build a shared instead of static library.
- DEBUG=1: build library with no optimizations, suitable for debugging. Debug library
	is called libwebsocketpp_dbg
- CXX=*: uses * as the c++ compiler instead of system default

Build tested on
- Mac OS X 10.7 with apple gcc 4.2, macports gcc 4.6, apple llvm/clang, boost 1.47
- Fedora 15, gcc 4.6, boost 1.46
- Ubunutu server, gcc,  boost 1.42

Outstanding issues
- Acknowledgement details
- Subprotocol negotiation interface
- check draft 14 issues
- session.cpp - add_header. Decide what should happen with multiple calls to 
   add header with the same key
- multiple headers of the same value
- Better exception model
- closing handshake reason/ error codes?
- tests for opening/closing handshake
- tests for utf8
- utf8 streaming validation
- more easily configurable frame size limit
- Better system of handling server wide defaults (like hosts, frame limits, etc)

To check
- double check bugs in autobahn (sending wrong localhost:9000 header) not 
- checking masking in the 9.x tests

Unimplemented features
- SSL
- frame or streaming based API
- client features
- extension negotiation interface

Acknowledgements
- Boost Asio and other libraries
- base64 library
- sha1 library
- htonll discussion
- build/makefile from libjson

- Autobahn test suite
- testing by Keith Brisson



API spec notes



## Server API ##
websocketpp.hpp

create a websocketpp::server_ptr initialized to a new websocketpp::server object

the server constructor will need three things.
- A boost::asio::io_service object to manage its async operations
- A boost::asio::ip::tcp::endpoint to listen to for new connections
- An object that implements the websocketpp::connection_handler interface to provide callback functions (See Handler API)

After construction, the server object will be in the initialization state. 
At this time you can set up server configuration options either via calling individual set option commands
or by loading them in a batch from a configuration file.

The only required option is that at least one host value must be set. 
Incoming websocket connections must specify a host value that they wish to connect to 
and if the server object does not have that host value in it's list of canonical hosts it will reject the connection.

[note about settings that can be changed live?]

Once the server has been configured the way you want, call the start_accept() method. 
This will add the first async call to your io_service. If your io_service was already running, 
the server will start accepting connections immediately. If not you will need to call io_service.run() to start it.

Once the server has started it will accept new connections. 
A new session object will be created for each connection accepted. 
The session will perform the websocket handshake and if it is successful begin reading frames. 
The session will continue reading frames until an error occurs or a connection close frame is seen. 
The session will notify the handler that it was initialized with (see Handler API) as necessary. 
The Session API defines how a handler (or other part of the end application) can interact with the session 
(to get information about the session, send messages back to the client, etc).

## Client API ##
include websocketpp.hpp

create a websocketpp::client_ptr initialized to a new websocketpp::client object

the client constructor will need:
- A boost::asio::io_service object to manage its async operations
- An object that implements the websocketpp::connection_handler interface to provide callback functions (See Handler API)

After construction, the client object will be in the initialization state. 
At this time you can set up client configuration options either via calling individual set options commands 
or by loading them in a batch from a configuration file.

Opening a new connection:
Per the websocket spec, a client can only have one connection in the connecting state at a time. 
Client method new_session() will create a new session and return a shared pointer to it. 
After this point new_session will throw an exception if you attempt to call it again 
before the most recently created session has either successfully connected or failed to connect. 
new_session()
- call websocketpp::client::new_session(). This will return a session_ptr


## Handler API ##
The handler API defines the interface that a websocketpp session will use to communicate information 
about the session state and new messages to your application.

A client or server must be initialized with a default handler that will be used for all sessions. 
The default handler may pass a session off to another handler as necessary.

A handler must implement the following methods:
- validate(session_ptr)
- on_fail(session_ptr)
- on_open(session_ptr)
- on_close(session_ptr)
- on_message(session_ptr,const std::vector<unsigned char> &)
- on_message(session_ptr,const std::string &)

validate will be called after a websocket handshake has been received and before it is accepted. 
It provides a handler with the ability to refuse a connection based on application specific logic 
(ex: restrict domains or negotiate subprotocols). To reject the connection throw a handshake_error. 
Validate is never called for client sessions. 
To refuse a client session (ex: if you do not like the set of extensions/subprotocols the server chose) 
you can close the connection immediately in the on_open member function.

on_fail is called whenever a session is terminated or failed before it was successfully established. 
This happens if there is an error during the handshake process or if the server refused the connection. 

on_fail will be the last time a session calls its handler. 
If your application will need information from `session` after this function you should either 
save the session_ptr somewhere or copy the data out.

on_open is called after the websocket session has been successfully established and is in the OPEN state. 
The session is now available to send messages and will begin reading frames 
and calling the on_message/on_close/on_error callbacks. 
A client may reject the connection by closing the session at this point.

on_close is called whenever an open session is closed for any reason. 
This can be due to either endpoint requesting a connection close or an error occurring. 
Information about why the session was closed can be extracted from the session itself.
on_close will be the last time a session calls its handler. 
If your application will need information from `session` after this function you should either 
save the session_ptr somewhere or copy the data out.

on_message (binary version) will be called when a binary message is received. 
Message data is passed as a vector of bytes (unsigned char). 
Data will not be available after this callback ends so the handler must either 
completely process the message or copy it somewhere else for processing later.

TODO: Notes about thread safety


## Session API ##
The Session API allows a handler to look up information about a session 
as well as interact with that session (send messages, close the connection, etc).

Session pointers are returned with every handler callback as well as every call to websocketpp::client::connect.


Handler Interface:
- set_handler(connection_handler_ptr)

Handshake Interface:
For the client these methods are valid after the server's handshake has been received. 
This is guaranteed to be the case by the time `on_open` is called.

For the server these methods are valid after the client's handshake has been received. 
This is guaranteed to be the case by the time `validate` is called.

- const std::string& get_subprotocol() const;
- const std::string& get_resource() const;
- const std::string& get_origin() const;
- std::string get_client_header(const std::string&) const;
- std::string get_server_header(const std::string&) const;
- const std::vector<std::string>& get_extensions() const;
- unsigned int get_version() const;

Frame Interface
- void send(const std::string &);
- void send(const std::vector<unsigned char> &);
- void ping(const std::string &);
- void pong(const std::string &);

These methods are valid only for open connections. They will throw an exception if called from any other state.

WebSocket++ does not queue messages. As such only one send operation can be occurring at once. 
TODO: failure behavior. OPTIONS:
- send will throw a `session_busy` exception if busy
- send will return true/false
- a callback could be defined letting the handler know that it is safe to write again.

Session Interface
- void close(uint16_t status,const std::string &reason);
- bool is_server() const;




--------------------------------
screwing around with a policy based refactoring
--------------------------------

template<class WebSocketRole,class Logger>
class endpoint : public WebSocketRole, Logger {
public:
	endpoint(connection_handler_ptr);
	
	size_t get_connected_client_count() const;
	
	void set_endpoint(const tcp::endpoint& endpoint); // asio::bind
	
private:
	std::list<session_ptr>	m_connections;
	connection_handler_ptr	m_handler;
	
	boost::asio::io_service	m_io_service;
	tcp::acceptor			m_acceptor;
}

class server_interface {
public:
	void add_host(const std::string &host);
	void remove_host(const std::string &host);
	bool validate_host(const std::string &host) const;
	
	void set_max_message_size(uint64_t size);
	bool validate_message_size(uint64_t size) const;
	
	void start() {
		// start_accept()
		// io_service.run()
	}
private:
	void start_accept();
	void handle_accept(session_ptr session, const boost::system::error_code&)
	
	std::set<std::string>	m_hosts;
	uint64_t				m_max_message_size;
}

class client_interface {
public:
	session_ptr connect(const std::string &url);
protected:
	
private:

}



template<class HandshakePolicy>
class session : public HandshakePolicy {
public:
	session();
private:
	
}

namespace websocketpp {
namespace handshake {

/* a handshake policy must define:
void on_connect();
bool is_server() const;

*/

class server {
	
}

class client {
	
}

} // handshake
} // websocketpp