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
- Mac OS X 10.7 with apple gcc 4.2, macports gcc 4.6, apple llvm/clang




Outstanding issues
- Acknowledgement details
- Add license information to each file
- Subprotocol negotiation interface
- check draft 13 issues
- session.cpp - add_header. Decide what should happen with multiple calls to 
   add header with the same key
- multiple headers of the same value
- Better exception model
- closing handshake reason/ error codes?

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

