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
- Subprotocol negotiation
- Hybi 00/Hixie 76 legacy protocol support
- Performance tuning
- Outgoing Proxy Support
- PowerPC support
- Visual Studio / Windows support

Implimented, API not finalized
- open_handler
- close_handler
- validate_handler
- http_handler

Needs work:
- Timeouts

Non-release blocking feature roadmap
- Extension support
- permessage_compress extension
- Message buffer pool
- CMake build/install support