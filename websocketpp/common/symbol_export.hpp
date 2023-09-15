#ifndef WEBSOCKETPP_COMMON_SYMBOL_EXPORT_HPP
#define WEBSOCKETPP_COMMON_SYMBOL_EXPORT_HPP

/**
 * Defines `_WEBSOCKETPP_SYMBOL_EXPORT`, used to export certain symbols, such
 * as exception types and `lib::error_category` singletons, that do not work
 * correctly across shared library boundaries otherwise.
 */

#if !defined(_WEBSOCKETPP_SYMBOL_EXPORT) && !defined(_WEBSOCKETPP_NO_SYMBOL_EXPORT)
    // `_WEBSOCKETPP_SYMBOL_EXPORT` can be predefined, or an empty definition
    // can be forced by defining `_WEBSOCKETPP_NO_SYMBOL_EXPORT`.

    #if defined(_WEBSOCKETPP_BOOST_SYMBOL_EXPORT)
        // Just use `BOOST_SYMBOL_EXPORT` from Boost.Config .

        #include <boost/config.hpp>
        #define _WEBSOCKETPP_SYMBOL_EXPORT BOOST_SYMBOL_EXPORT

    // The following is fairly portable, but there are some environments in
    // which it might not work. If so, predefining `_WEBSOCKETPP_SYMBOL_EXPORT`
    // or opting out entirely with `_WEBSOCKETPP_NO_SYMBOL_EXPORT` are viable
    // alternatives.
    #elif defined(__GNUC__) || defined(__clang__)
        #if defined(_WIN32) || defined(__CYGWIN__)
            #define _WEBSOCKETPP_SYMBOL_EXPORT __attribute__((dllexport))
        #else
            #define _WEBSOCKETPP_SYMBOL_EXPORT __attribute__((visibility("default")))
        #endif
    #elif defined(_WIN32)
        #define _WEBSOCKETPP_SYMBOL_EXPORT __declspec(dllexport)
    #endif
#endif

#ifndef _WEBSOCKETPP_SYMBOL_EXPORT
    #define _WEBSOCKETPP_SYMBOL_EXPORT
#endif

#endif
