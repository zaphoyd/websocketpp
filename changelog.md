HEAD
- Refactors URI to be exception free and not use the regular expressions. This
  eliminates the dependency on boost or C++11 regex libraries.
- Updates handling of Server and User-Agent headers
- Fix issue where pong timeout handler always fired. Thank you Steven Klassen 
  for reporting this bug.
- Add ping and pong endpoint wrapper methods
- Add `get_request()` pass through method to connection to allow calling methods
  specific to the HTTP policy in use.
- Fix issue compiling with `WEBSOCKETPP_STRICT_MASKING`
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