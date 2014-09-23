HEAD

0.3.0 - 2014-08-10
- Feature: Adds `start_perpetual` and `stop_perpetual` methods to asio transport
  These may be used to replace manually managed `asio::io_service::work` objects
- Feature: Allow setting pong and handshake timeouts at runtime.
- Feature: Allows changing the listen backlog queue length.
- Feature: Split tcp init into pre and post init.
- Feature: Adds URI method to extract query string from URI. Thank you Banaan
  for code. #298
- Feature: Adds a compile time switch to asio transport config to disable
  certain multithreading features (some locks, asio strands)
- Feature: Adds the ability to pause reading on a connection. Paused connections
  will not read more data from their socket, allowing TCP flow control to work
  without blocking the main thread.
- Feature: Adds the ability to specify whether or not to use the `SO_REUSEADDR`
  TCP socket option. The default for this value has been changed from `true` to
  `false`.
- Feature: Adds the ability to specify a maximum message size.
- Feature: Adds `close::status::get_string(...)` method to look up a human
  readable string given a close code value.
- Feature: Adds `connection::read_all(...)` method to iostream transport as a
  convenience method for reading all data into the connection buffer without the
  end user needing to manually loop on `read_some`.
- Improvement: Open, close, and pong timeouts can be disabled entirely by
  setting their duration to 0.
- Improvement: Numerous performance improvements. Including: tuned default
  buffer sizes based on profiling, caching of handler binding for async
  reads/writes, non-malloc allocators for read/write handlers, disabling of a
  number of questionably useful range sanity checks in tight inner loops.
- Improvement: Cleaned up the handling of TLS related errors. TLS errors will
  now be reported with more detail on the info channel rather than all being
  `tls_short_read` or `pass_through`. In addition, many cases where a TLS short
  read was in fact expected are no longer classified as errors. Expected TLS
  short reads and quasi-expected socket shutdown related errors will no longer
  be reported as unclean WebSocket shutdowns to the application. Information
  about them will remain in the info error channel for debugging purposes.
- Improvement: `start_accept` and `listen` errors are now reported to the caller
  either via an exception or an ec parameter.
- Improvement: Outgoing writes are now batched for improved message throughput
  and reduced system call and TCP frame overhead.
- Bug: Fix some cases of calls to empty lib::function objects.
- Bug: Fix memory leak of connection objects due to cached handlers holding on to
  reference counted pointers. #310 Thank you otaras for reporting.
- Bug: Fix issue with const endpoint accessors (such as `get_user_agent`) not
  compiling due to non-const mutex use. #292 Thank you logofive for reporting.
- Bug: Fix handler allocation crash with multithreaded `io_service`.
- Bug: Fixes incorrect whitespace handling in header parsing. #301 Thank you
  Wolfram Schroers for reporting
- Bug: Fix a crash when parsing empty HTTP headers. Thank you Thingol for
  reporting.
- Bug: Fix a crash following use of the `stop_listening` function. Thank you
  Thingol for reporting.
- Bug: Fix use of variable names that shadow function parameters. The library
  should compile cleanly with -Wshadow now. Thank you giszo for reporting. #318
- Bug: Fix an issue where `set_open_handshake_timeout` was ignored by server
  code. Thank you Robin Rowe for reporting.
- Bug: Fix an issue where custom timeout values weren't being propagated from
  endpoints to new connections.
- Bug: Fix a number of memory leaks related to server connection failures. #323
  #333 #334 #335 Thank you droppy and aydany for reporting and patches.
  reporting.
- Compatibility: Fix compile time conflict with Visual Studio's MIN/MAX macros.
  Thank you Robin Rowe for reporting.
- Documentation: Examples and test suite build system now defaults to clang on
  OS X

0.3.0-alpha4 - 2013-10-11
- HTTP requests ending normally are no longer logged as errors. Thank you Banaan
  for reporting. #294
- Eliminates spurious expired timers in certain error conditions. Thank you
  Banaan for reporting. #295
- Consolidates all bundled library licenses into the COPYING file. #294
- Updates bundled sha1 library to one with a cleaner interface and more
  straight-forward license. Thank you lotodore for reporting and Evgeni Golov
  for reviewing. #294
- Re-introduces strands to asio transport, allowing `io_service` thread pools to
  be used (with some limitations).
- Removes endpoint code that kept track of a connection list that was never used
  anywhere. Removes a lock and reduces connection creation/deletion complexity
  from O(log n) to O(1) in the number of connections.
- A number of internal changes to transport APIs
- Deprecates iostream transport `readsome` in favor of `read_some` which is more
  consistent with the naming of the rest of the library.
- Adds preliminary signaling to iostream transport of eof and fatal transport
  errors
- Updates transport code to use shared pointers rather than raw pointers to
  prevent asio from retaining pointers to connection methods after the
  connection goes out of scope. #293 Thank you otaras for reporting.
- Fixes an issue where custom headers couldn't be set for client connections
  Thank you Jerry Win and Wolfram Schroers for reporting.
- Fixes a compile error on visual studio when using interrupts. Thank you Javier
  Rey Neira for reporting this.
- Adds new 1012 and 1013 close codes per IANA registry
- Add `set_remote_endpoint` method to iostream transport.
- Add `set_secure` method to iostream transport.
- Fix typo in .gitattributes file. Thank you jstarasov for reporting this. #280
- Add missing locale include. Thank you Toninoso for reporting this. #281
- Refactors `asio_transport` endpoint and adds full documentation and exception
  free varients of all methods.
- Removes `asio_transport` endpoint method cancel(). Use `stop_listen()` instead
- Wrap internal `io_service` `run_one()` method
- Suppress error when trying to shut down a connection that was already closed

0.3.0-alpha3 - 2013-07-16
- Minor refactor to bundled sha1 library
- HTTP header comparisons are now case insensitive. #220, #275
- Refactors URI to be exception free and not use regular expressions. This
  eliminates the dependency on boost or C++11 regex libraries allowing native
  C++11 usage on GCC 4.4 and higher and significantly reduces staticly built
  binary sizes.
- Updates handling of Server and User-Agent headers to better handle custom
  settings and allow suppression of these headers for security purposes.
- Fix issue where pong timeout handler always fired. Thank you Steven Klassen
  for reporting this bug.
- Add ping and pong endpoint wrapper methods
- Add `get_request()` pass through method to connection to allow calling methods
  specific to the HTTP policy in use.
- Fix issue compile error with `WEBSOCKETPP_STRICT_MASKING` enabled and another
  issue where `WEBSOCKETPP_STRICT_MASKING` was not applied to incoming messages.
  Thank you Petter Norby for reporting and testing these bugs. #264
- Add additional macro guards for use with boost_config. Thank you breyed
  for testing and code. #261

0.3.0-alpha2 - 2013-06-09
- Fix a regression that caused servers being sent two close frames in a row
  to end a connection uncleanly. #259
- Fix a regression that caused spurious frames following a legitimate close
  frames to erroneously trigger handlers. #258
- Change default HTTP response error code when no http_handler is defined from
  500/Internal Server Error to 426/Upgrade Required
- Remove timezone from logger timestamp to work around issues with the Windows
  implimentation of strftime. Thank you breyed for testing and code. #257
- Switch integer literals to char literals to improve VCPP compatibility.
  Thank you breyed for testing and code. #257
- Add MSVCPP warning suppression for the bundled SHA1 library. Thank you breyed
  for testing and code. #257

0.3.0-alpha1 - 2013-06-09
- Initial Release
