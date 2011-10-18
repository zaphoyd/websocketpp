How to use this library

WebSocket++ is a C++ websocket server library implimented using the Boost Asio networking stack. It is designed to provide a simple interface

Built on Asio's proactor asyncronious event loop. 

Building a program using WebSocket++ has two parts
1. Impliment a connection handler.
This is done by subclassing websocketpp::connection_handler. Each websocket connection is attached to a connection handler. The handler impliments the following methods:

validate:
Called after the client handshake is recieved but before the connection is accepted. Allows cookie authentication, origin checking, subprotocol negotiation, etc

connect:
Called when the connection has been established and writes are allowed.

disconnect:
Called when the connection has been disconnected

message: text and binary variants
Called when a new websocket message is recieved.

The handler has access to the following websocket session api:
get_header
returns the value of an HTTP header sent by the client during the handshake

get_request
returns the resource requested by the client in the handshake

set_handler:
pass responsibility for this connection to another connection handler

set_http_error:
reject the connection with a specific HTTP error

add_header
adds an HTTP header to the server handshake

set_subprotocol
selects a subprotocol for the connection

send: text and binary varients
send a websocket message

ping:
send a ping

2. Start Asio's event loop with a TCP endpoint and your connection handler

There are two example programs in the examples directory that demonstrate this use pattern. One is a trivial stateless echo server, the other is a simple web based chat client. Both include example javascript clients. The echo server is suitable for use with automated testing suites such as the Autobahn test suite.

By default, a single connection handler object is used for all connections. If needs require, that default handler can either store per-connection state itself or create new handlers and pass off responsibility for the connection to them.

How to build this library

Build static library
make

Build and install in system include directories
make install

Avaliable flags:
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

Unimplimented features
- SSL
- frame or streaming based api
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


6.59


Server API
websocketpp.hpp

create a websocketpp::server_ptr initialized to a new websocketpp::server object

the server constructor will need three things.
- A boost::asio::io_service object to use to manage its async operations
- A boost::asio::ip::tcp::endpoint to listen to for new connections
- An object that impliments the websocketpp::connection_handler interface to provide callback functions (See Handler API)

After construction the server object will be in the initialization state. At this time you can set up server config options either via calling individual set option commands or by loading them in a batch from a config file.

The only required option is that at least one host value must be set. Incoming websocket connections must specify a host value that they wish to connect to and if the server object does not have that host value in it's list of canonical hosts it will reject the connection.

[note about settings that can be changed live?]

Once the server has been configured the way you want, call the start_accept() method. This will add the first async call to your io_service. If your io_service was already running, the server will start accepting connections immediately. If not you will need to call io_service.run() to start it.

Client API
include websocketpp.hpp

create a websocketpp::client_ptr initialized to a new websocketpp::client object

the client constructor will need:
- A boost::asio::io_service object to use to manage its async operations
- An object that impliments the websocketpp::connection_handler interface to privde callback functions (See Handler API)

After construction, the client object will be in the initialization state. At this time you can set up client config options either via calling individual set options commands or by loading them in a batch from a config file.

Opening a new connection:
Per the websocket spec, a client can only have one connection in the connecting state at a time. Client method new_session() will create a new session and return a shared pointer to it. After this point new_session will throw an exception if you attempt to call it again before the most recently created session has either successfully connected or failed to connect. new_session()
- call websocketpp::client::new_session(). This will return a session_ptr


Handler API




Session API





