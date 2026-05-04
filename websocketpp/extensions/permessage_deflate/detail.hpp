/*
 * Copyright (c) 2015, Peter Thorson. All rights reserved.
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

#ifndef WEBSOCKETPP_PROCESSOR_EXTENSION_PERMESSAGEDEFLATE_DETAIL_HPP
#define WEBSOCKETPP_PROCESSOR_EXTENSION_PERMESSAGEDEFLATE_DETAIL_HPP

#include <websocketpp/common/stdint.hpp>

#include <cstddef>

namespace websocketpp {
namespace extensions {
namespace permessage_deflate {
namespace detail {

// Compile-time detection helpers for optional members of an extension's
// configuration struct. Each detector pairs a "has_X" trait with an
// "X_or_default" dispatcher; together they let the extension consume an
// optional config constant without forcing every Config to define it.
//
// The detection idiom: declare two overloads of a function template. The
// first uses a defaulted non-type template argument that references the
// candidate member; if the member doesn't exist on T, substitution fails
// (SFINAE) and the overload silently drops out of the candidate set. The
// second is a variadic catch-all. Pick between them with sizeof on the
// return type — the call itself never happens at runtime.
//
// The dispatcher uses partial specialization rather than a runtime branch
// so the missing-member case never instantiates a reference to T::member.

// --- max_message_size --------------------------------------------------------

template <typename T, size_t = T::max_message_size>
char has_max_message_size_test(int);
template <typename>
long has_max_message_size_test(...);

template <typename T>
struct has_max_message_size {
    static bool const value =
        sizeof(has_max_message_size_test<T>(0)) == sizeof(char);
};

template <typename Config, size_t Default,
          bool = has_max_message_size<Config>::value>
struct max_message_size_or_default {
    static size_t get() { return Default; }
};

template <typename Config, size_t Default>
struct max_message_size_or_default<Config, Default, true> {
    static size_t get() { return Config::max_message_size; }
};

} // namespace detail
} // namespace permessage_deflate
} // namespace extensions
} // namespace websocketpp

#endif // WEBSOCKETPP_PROCESSOR_EXTENSION_PERMESSAGEDEFLATE_DETAIL_HPP
