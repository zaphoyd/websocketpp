/*
 * Copyright (c) 2012, Peter Thorson. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the WebSocket++ Project nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL PETER THORSON BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#ifndef WEBSOCKETPP_HPP
#define WEBSOCKETPP_HPP

/*

Endpoint
- container for connections
- Store and forward default connection settings

Connection
- Represents the state and functionality of a single WebSocket session starting 
  with the opening handshake and completing with the closing one.
- After a connection is created settings may be applied that will be used for
  this connection. 
- Once setup is complete a start method is run and the connection enters its 
  event loop. The connection requests bytes from its transport, then runs those
  bytes through the appropriate websocket frame processor, and calls handler
  methods appropriate for the types of frames recieved.





Policies:

Concurrency






Concurrency Models
Single Thread Async (lock free)
- WS++ runs lock free (Access to endpoint and connection from non-WS++ threads is unsafe)
- All handlers and networking operations run in a single thread.
- Handlers can block each other and network operations
- Good for low traffic workflows where connections are independent and requests are short.

Single Thread Async
- Same as lock free version except access to endpoint and connection from non-WS++ threads is safe
- Good for workflows where any long running handler job is deferred to a non-WS++ thread for processing.

Thread Pool (lock free)
- WS++ runs lock free (access to endpoint and connection from non-WS++ threads is unsafe)
- Handlers and networking operations invoked by multiple threads. Individual connections are serialized.
- n handlers will block network operations (n=num_threads)
- Allows much better multi-core utilization, does not require end user syncronization as long as all work is performed inside handlers and handlers only reference their own connection. Handler local data must be syncronized.

Thread pool
- Same as lock free version except access to endpoint and connection from non-WS++ threads is safe.

Thread per connection
- 

io_service policies
- external vs internal
- per endpoint or per connection


message policies?
- Control Messages:
-- Each connection should have a single control message permanently allocated
- Data Messages
-- Dynamically allocate a new data message as needed.
-- Re-usable pool of data messages per endpoint
-- Re-usable pool of data messages per connection


*/


#include <websocketpp/common.hpp>
#include <websocketpp/concurrency/async.hpp>

#include <websocketpp/connection.hpp>
#include <websocketpp/endpoint.hpp>

#endif // WEBSOCKETPP_ENDPOINT_HPP