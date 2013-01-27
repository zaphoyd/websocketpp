Experimental 0.3.x branch of WebSocket++

This branch really is experimental and should not be used as the basis for any
serious projects. Its API is likely to change rapidly in the next few weeks.

Complete & Tested:
- Server role passes all Autobahn v0.5.9 test suite tests strictly
- Streaming UTF8 validation

Implimented but needs more testing
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

Needs work:
- random number generation
- open_handler
- close_handler
- validate_handler
- http_handler
- Client role
- Extension support
- permessage_compress extension
