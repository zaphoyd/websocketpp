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

#ifndef HTTP_PROXY_AUTHENTICATOR_HPP
#define HTTP_PROXY_AUTHENTICATOR_HPP

#include <websocketpp/common/memory.hpp>

#include <websocketpp/http/response.hpp>

#include <string>
#include <algorithm>
#include <locale>
#include <cctype>

namespace websocketpp {
    namespace http {
        namespace proxy {

            /// The 'proxy_authenticator' manages parsing and tokens required for proxy autentication.
            /**
            * The proxy authenticator handles http proxy authentication. It supports 'Basic', 'NTLM'
            * and 'Negotiate' authentation - depending on the security context object used. Currently 
            * there is a Win32 security context which will authenticate using the signed on users
            * credentials for NTLM and Negotiate. 
            *
            * Where the proxy supports multiple different auth schemes, the proxy authenticator will 
            * select the scheme using the following priority:
            *   1. Negotiate
            *   2. NTLM
            *   3. Digest (not supported just now)
            *   4. Basic
            *  
            */
            template <typename security_context>
            class proxy_authenticator {
            private:
                typedef typename security_context::Ptr security_context_ptr;

                std::string m_proxy;
                std::string m_auth_scheme_name;
                std::string m_auth_token;
                bool authenticated=false;

                struct
                {
                    std::string username;
                    std::string password;

                } m_basic_auth;

                security_context_ptr m_security_context;

                std::string build_auth_response() {
                    if (!m_auth_scheme_name.empty() && !m_auth_token.empty()) {
                        return m_auth_scheme_name + " " + m_auth_token;
                    }

                    return "";
                }

            public:
                typedef lib::shared_ptr<proxy_authenticator> ptr;

                /// Construct a 'proxy_authenticator'
                /**
                * Constructs a proxy authenticator fo a given proxy
                *
                * @param proxy: Complete proxy URI eg http://proxy.example.com:8080/
                */
                proxy_authenticator(std::string const& proxy) : m_proxy(proxy) {
                }

                /// Set Basice authentication credentials
                /**
                * This method sets the basic auth credentials. This can be used for Basic 
                * authentication, and Digest, however only Basic is supported just now.
                *
                * @param username: username used in the auth response.
                * @param password: password used in the aut response
                */
                void set_basic_auth(std::string const& username, std::string const& password)
                {
                    m_basic_auth.username = username;
                    m_basic_auth.password = password;
                }

                /// Calculate the next auth token
                /**
                * Using the reponse from the proxy, be that the initial response with a list
                * of auth schemes, or a subsequent response with a scheme and a challenge token,
                * this method will calculate the next auth token to be used. 
                *
                * @param auth_headers: Response headers from 'Proxy-Authenticate'
                *
                * Return: ture:  New token calculated, continue auth flow.
                *         false: No new token calculated - fail the auth flow
                */
                bool next_token(std::string const& auth_headers);

                /// Return the next calculated auth token
                /**
                * This method will return the next calculated auth token for use in the 
                * 'Proxy-Authorization' header field.
                *
                * Return: New Auth token for 'Proxy-Authorization' header.
                */
                std::string get_auth_token() {
                    return build_auth_response();
                }

                /// To be called after 'proxy_authentication' is complete
                /**
                * Marks this authenticator as complete, which means 'get_authenticated_token()
                * returns the valid token for proxy authentication. 
                */
                void set_authenticated() {
                    authenticated = true;
                }

                /// Returns the authenticated token after auth complete
                /**
                * 
                */
                std::string get_authenticated_token() {
                    return authenticated ? build_auth_response() : "";
                }

                /// Returns the proxy uri
                /**
                * 
                */
                std::string get_proxy() {
                    return m_proxy;
                }

            };

        }   // namespace proxy
    }       // namespace http
}           // namespace websocketpp

#include <websocketpp/http/impl/proxy_authenticator_impl.hpp>

#endif // HTTP_PROXY_AUTHENTICATOR_HPP
