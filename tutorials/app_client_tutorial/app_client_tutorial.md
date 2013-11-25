Client

separate thread

multiple connections at once


An application with its own event loop. This demonstrates an example of how to integrate WebSocket++ into an application with a pre-existing event loop. This includes applications that use GUI frameworks like wxWidgets, Qt, etc.

- Uses a dedicated thread (or thread pool) to process WebSocket traffic. This allows better responsiveness for WebSocket activities, multiple similtaneous connections, easily reconnecting 


This example application impliments a command line menu to manage an arbitrary number of outgoing websocket connections.


Core Options:
- Create a new connection to a WebSocket server with the given URI.
- View list of outstanding connections with some metadata
- Send a message along a specific connection
- Send a message to all connections
- List all messages received by a given connection
- Close a specific connection
- Close all connections

Bonus Features
- Set the user agent
- Set the origin
- Set other arbitrary headers
- Set proxy information
- Send a ping

utility features
- Pipe one connection to another
