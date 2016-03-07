/*
 * Copyright (c) 2016, Peter Thorson. All rights reserved.
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
 * The initial version of this Security Context policy was contributed to the WebSocket++
 * project by Colie McGarry.
 */

#ifndef WEBSOCKETPP_COMMON_SECURITY_CONTEXT_HPP
#define WEBSOCKETPP_COMMON_SECURITY_CONTEXT_HPP

namespace websocketpp {
    namespace lib {
        namespace security {
            class SecurityContext
            {
            public:
                using Ptr = std::shared_ptr<SecurityContext>;

                static Ptr build(const std::string& , const std::string& )  { return  Ptr(); }

                SecurityContext(const std::string& , const std::string& )   { }

                bool nextAuthToken(const std::string&)                      { return ""; }
                std::string getUpdatedToken() const                         { return ""; }
            };
        }       // security
    }           // lib
}               // websocket

#endif // WEBSOCKETPP_COMMON_SECURITY_CONTEXT_HPP
