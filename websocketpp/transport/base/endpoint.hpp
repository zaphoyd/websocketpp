/*
 * Copyright (c) 2013, Peter Thorson. All rights reserved.
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

#ifndef WEBSOCKETPP_TRANSPORT_BASE_HPP
#define WEBSOCKETPP_TRANSPORT_BASE_HPP

#include <websocketpp/common/cpp11.hpp>
#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/common/functional.hpp>
#include <websocketpp/common/system_error.hpp>
#include <websocketpp/uri.hpp>

#include <iostream>

namespace websocketpp {
namespace transport {

// Endpoint callbacks
typedef lib::function<void(connection_hdl,const lib::error_code&)> accept_handler;
typedef lib::function<void(connection_hdl,const lib::error_code&)> connect_handler;

typedef lib::function<void()> endpoint_lock;

// Endpoint interface
// Methods a transport endpoint must impliment

/// Initialize a connection
/**
 * Signature: lib::error_code init(transport_con_ptr tcon);
 * 
 * init is called by an endpoint once for each newly created connection. 
 * It's purpose is to give the transport policy the chance to perform any 
 * transport specific initialization that couldn't be done via the default 
 * constructor.
 *
 * @param tcon A pointer to the transport portion of the connection.
 *
 * @return A status code indicating the success or failure of the operation
 */

} // namespace transport
} // namespace websocketpp

#endif // WEBSOCKETPP_TRANSPORT_BASE_HPP
