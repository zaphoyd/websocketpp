HEAD
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
