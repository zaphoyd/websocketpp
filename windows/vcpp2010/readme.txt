Build Boost
===========

Prerequisites: Visual Sutdio C ++ 2010 Express (or higher)

Download Boost from http://www.boost.org/

Unzip boost_1_47_0.zip to C:\boost_1_47_0

Open a "Visual Studio Command Prompt (2010)" (not a regular cmd.exe!).

cd C:\boost_1_47_0

bootstrap

.\b2 runtime-link=static

Now set a system environment variable:

BOOSTROOT = C:\boost_1_47_0


Background:

 - http://www.boost.org/doc/libs/1_47_0/more/getting_started/windows.html
 - http://www.boost.org/doc/libs/1_47_0/more/getting_started/windows.html#library-naming
 - http://stackoverflow.com/questions/2035287/static-runtime-library-linking-for-visual-c-express-2008


Build websocket++
=================

Open websocketpp.sln in VS.

Build Solution (F7).
