Utility Client Example Application
==================================

Initial Setup
-------------

### Step 1

A basic program loop that prompts the user for a command and then processes it.

Build:
clang++ chapter1.cpp

### Step 2

Add WebSocket++ includes and set up endpoint type.

WebSocket++ includes two major object types. The endpoint and the connection. The
endpoint creates and launches new connections and maintains default settings for
those connections. Endpoints also manage any shared network resources.

The connection stores information specific to each connection. 

Aside:
Once a connection is launched, there is no link between the endpoint and the connection. All default settings are copied into the new connection by the endpoint. Changing default settings on an endpoint will only affect future connections.
Connections do not maintain a link back to their associated endpoint. Endpoints do not maintain a list of outstanding connections. If your application needs to iterate over all connections it will need to maintain a list of them itself.

Terminology:
WebSocket++ endpoints have a group of settings that may be configured at compile time via the `config` template arguement. A config is a struct that contains types and static constants that is used to produce an endpoint with specific properties. Depending on which config is being used the endpoint will have different methods available and may have additional third party dependencies. 
Configs will be discussed later in more detail, for now we are going to use one of the default configs, `asio_no_tls_client`. This is a client config that uses boost::asio to provide network transport and does not support TLS based security. Later on we will discuss how to introduce TLS based security into a WebSocket++ application.

The following line includes the asio_no_tls_client config. All of the default library configs are located in `websocketpp/config/*`
`#include <websocketpp/config/asio_no_tls_client.hpp>`

The following line includes the WebSocket++ client endpoint itself:
`#include <websocketpp/client.hpp>`

The following line configures the client template with the config type to define the type of the endpoint that will be used in our program.
`typedef websocketpp::client<websocketpp::config::asio_client> client`

Build:
Adding WebSocket++ has added a few dependencies to our program now that must be addressed in the build system. Firstly, the WebSocket++ and Boost library headers must be in the include search path of your build system. How exactly this is done depends on where you have the WebSocket++ headers installed and what build system you are using.

In addition to the new headers, boost::asio depends on the boost_system shared library. This will need to be added (either as a static or dynamic) to the linker. Refer to your build environment documentation for instructions on linking to shared libraries.

clang++ step2.cpp -lboost_system

### Step 3

Create endpoint wrapper object that handles initialization and setting up the background thread.

The websocket_endpoint class includes as members a client of the type we defined earlier and a thread that will be used to run network operations.

Aside:
Note the use of types in the websocketpp::lib namespace. Specifically websocketpp::lib::shared_ptr and websocketpp::lib::thread. In addition, note the inclusion of two additional headers from the <websocketpp/common/*> directory. These types are wrappers used here to allow the example to be built either against the boost libraries or the C++11 standard library.
For best results, in your application you should replace websocketpp::lib::* types with the types appropriate to your environment. For example, if you plan to use the boost versions of these types you would include: boost/shared_ptr.hpp and boost/thread.hpp and use boost::shared_ptr and boost::thread respectively. If you plan to use your environments C++11 standard library you would include <memory> and <thread> and use std::shared_ptr and std::thread. [TODO: link to more information about websocketpp::lib namespace]

Within the `websocket_endpoint` constructor several things happen:

The following sets the endpoint logging behavior to silent by clearing all of the access and error logging channels. [TODO: link to more information about logging]
```c++
m_endpoint.clear_access_channels(websocketpp::log::alevel::all);
m_endpoint.clear_error_channels(websocketpp::log::elevel::all);
```

The following lines perform an initialization of the transport system underlying the endpoint and sets it to perpetual mode. In perpetual mode the endpoint will not exit when it runs out of work. This is important because we want this endpoint to remain active while our application is running and process requests for new WebSocket connections on demand as we need them.
```c++
m_endpoint.init_asio();
m_endpoint.start_perpetual();
```

Finally, this line launches a thread to run the `run` method of our client endpoint.
```c++
m_thread.reset(new websocketpp::lib::thread(&client::run, &m_endpoint));
```

Build

Now that our client endpoint template is actually instantiated a few more linker dependencies will show up. In particular, WebSocket clients require a cryptographically secure random number generator. WebSocket++ is able to use either boost_random or the C++11 standard library <random> for this purpose.

