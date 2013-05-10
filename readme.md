WebSocket++ (0.3.x branch)
==========================

WebSocket++ is a header only C++ library that impliments RFC6455 The WebSocket
Protocol. It allows integrating WebSocket client and server functionality into
C++ programs. It uses interchangable network transport modules including one
based on C++ iostreams and one based on Boost Asio.

*This branch is no longer "experimental". It represents the current edge release
of the WebSocket++ library. The API of 0.3.x has some significant changes from
0.2.x, so care should be taken when upgrading.*

*This branch's API is relatively stable now. Features implimented so far are 
unlikely to change (except where explicitly noted). New features will be added
regularly until parity with the 0.2 branch is reached.*

*This is the preferred branch for new projects, especially those that involve 
multithreaded servers. It is better tested and documented. The 0.3.x API will 
be the basis for the 1.0 release.*

Major Features
==============
* Full support for RFC6455
* Partial support for Hixie 76 / Hybi 00, 07-17 draft specs (server only)
* Message/event based interface
* Supports secure WebSockets (TLS), IPv6, and explicit proxies.
* Flexible dependency management (C++11 Standard Library or Boost)
* Interchangable network transport modules (iostream and Boost Asio)
* Portible, cross platform and architecture design

Get Involved
============

**Project Website**  
http://www.zaphoyd.com/websocketpp/

**User Manual**  
http://www.zaphoyd.com/websocketpp/manual/

**GitHub Repository**  
https://github.com/zaphoyd/websocketpp/

**Announcements Mailing List**  
http://groups.google.com/group/websocketpp-announcements/

**Discussion / Development / Support Mailing List / Forum**  
http://groups.google.com/group/websocketpp/

Author
======
Peter Thorson - websocketpp@zaphoyd.com