WebSocket++ 0.3.x branch

This branch is no longer "experimental". It represents the current edge release
of the WebSocket++ library. The API of 0.3.x has some significant changes from
0.2.x, so care should be taken when upgrading.

This branch's API is relatively stable now. Features implimented so far are 
unlikely to change (except where explicitly noted). New features will be added
regularly until parity with the 0.2 branch is reached.

This is the preferred branch for new projects, especially those that involve 
multithreaded servers. It is better tested and documented. The 0.3.x API will 
be the basis for the 1.0 release.

Complete & Tested:
- Server and client roles pass all Autobahn v0.5.9 test suite tests strictly
- Streaming UTF8 validation
- random number generation
- iostream based transport
- C++11 support
- LLVM/Clang support
- GCC support
- 64 bit support
- 32 bit support

Implimented, needs more testing
- TLS support
- echo_server & echo_server_tls
- External io_service support
- socket_init_handler
- tls_init_handler
- message_handler
- ping_handler
- pong_handler
- tcp_init_handler
- exception/error handling
- Logging
- Client role

Implimented, API not finalized
- open_handler
- close_handler
- validate_handler
- http_handler

Needs work:
- PowerPC support
- Extension support
- permessage_compress extension
- Visual Studio / Windows support
- Hybi 00/Hixie 76 legacy protocol support
- Message buffer pool
- Performance tuning