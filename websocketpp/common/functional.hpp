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

#ifndef WEBSOCKETPP_COMMON_FUNCTIONAL_HPP
#define WEBSOCKETPP_COMMON_FUNCTIONAL_HPP

//  The workarounds for no C++ 11 smart pointers and function wrappers
//  only work if either both or neither is defined.
#if !defined BOOST_NO_CXX11_SMART_PTR && !defined BOOST_NO_CXX11_HDR_FUNCTIONAL
    #include <functional>

    // For Visual Studio 2012 or other compilers that use faux variadics and control it with _VARIADIC_MAX,
    // this tests that the argument count is sufficient for the websocketpp library.
    // If not, compilation fails with confusing error messages.
    #if defined _VARIADIC_MAX && _VARIADIC_MAX < 6
        #error "_VARIADIC_MAX must be at least 6. Higher values are acceptable, but slow compilation."
    #endif
#else
    #include <boost/bind.hpp>
    #include <boost/function.hpp>
#endif

namespace websocketpp {
namespace lib {

#if !defined BOOST_NO_CXX11_SMART_PTR && !defined BOOST_NO_CXX11_HDR_FUNCTIONAL
    using std::function;
    using std::bind;
    using std::ref;
    namespace placeholders = std::placeholders;
#else
    using boost::function;
    using boost::bind;
    using boost::ref;
    namespace placeholders {
        // TODO: there has got to be a better way than this
        using ::_1;
        using ::_2;
    }
#endif

} // namespace lib
} // namespace websocketpp

#endif // WEBSOCKETPP_COMMON_FUNCTIONAL_HPP
