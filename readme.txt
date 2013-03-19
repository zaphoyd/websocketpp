WebSocket++ is a C++ library that impliments RFC6455, the WebSocket protocol.
It can be used to build applications that feature WebSocket functionality.

NOTE: This is the 0.2.x version of WebSocket++. It is currently the master
branch as the most recent version (0.3.x) still has some significant feature
regressions. There some fairly significant API changes in 0.3.x so upgrading
should be done carefully. Due to these changes, the 0.3.x version is the
preferred branch for any new development, especially development that involves
multithreaded servers or C++11. 0.3.x can be found in the 'Experimental' branch
on GitHub. Once it gets closer to feature parity with 0.2 it will be promoted to
master.

For more information, please visit: http://www.zaphoyd.com/websocketpp/

0.2.x Features:

   * Fully supports RFC6455 protocol
   * Partial support for Hixie draft 76 and Hybi drafts 7-17
   * Easy to use message/event based API
   * Boost ASIO based asynchronous network core
   * Supports secure WebSockets (TLS) and IPv6
   * Fully passes the AutoBahn 0.4.10 test suite
   * Includes WebSocket performance and stress testing tools 
   * Open-source (BSD license)

