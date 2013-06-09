HEAD
- Removes timezone from logger timestamp to work around issues with the Windows
  implimentation of strftime. Thank you breyed for testing and code. #257
- Switches integer literals to char literals to improve VCPP compatibility.
  Thank you breyed for testing and code. #257
- Adds MSVCPP warning suppression for the bundled SHA1 library. Thank you breyed
  for testing and code. #257

0.3.0-alpha1 - 2013-06-09
- Initial Release