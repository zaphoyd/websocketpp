#define BOOST_TEST_MODULE http_authenticator
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>

#include <websocketpp/http/proxy_authenticator.hpp>

BOOST_AUTO_TEST_CASE(is_token68_char) {
    for (int i = 0x00; i <= 0xFF; i++)
    {
        bool expectedResult = false;

        expectedResult = ::isalnum(i) ? true : expectedResult;

        expectedResult = (i == '-') ? true : expectedResult;
        expectedResult = (i == '.') ? true : expectedResult;
        expectedResult = (i == '_') ? true : expectedResult;
        expectedResult = (i == '~') ? true : expectedResult;
        expectedResult = (i == '+') ? true : expectedResult;
        expectedResult = (i == '/') ? true : expectedResult;
        expectedResult = (i == '=') ? true : expectedResult;

        BOOST_CHECK(websocketpp::http::proxy::auth_parser::is_token68_char((unsigned char)(i)) == expectedResult);
    }
}

BOOST_AUTO_TEST_CASE(auth_scheme_parser) {
    // Valid Basic Auth - with quoted string
    std::string auth_headers = "Basic realm=\"some realm with \\\"quoted string\\\"\",type=1";

    auto auth_schemes = websocketpp::http::proxy::auth_parser::parse_auth_schemes(auth_headers.begin(), auth_headers.end());

    BOOST_CHECK(auth_schemes.size() == 1);
    BOOST_CHECK(auth_schemes.front().is_basic());
    BOOST_CHECK(auth_schemes.front().get_realm() == "some realm with \"quoted string\"");

    // NTLM
    auth_headers = "NTLM";

    auth_schemes = websocketpp::http::proxy::auth_parser::parse_auth_schemes(auth_headers.begin(), auth_headers.end());

    BOOST_CHECK(auth_schemes.size() == 1);
    BOOST_CHECK(auth_schemes.front().is_ntlm());
    BOOST_CHECK(auth_schemes.front().get_challenge().empty());

    // NTLM with Challenge
    auth_headers = "NTLM challengeString=";

    auth_schemes = websocketpp::http::proxy::auth_parser::parse_auth_schemes(auth_headers.begin(), auth_headers.end());

    BOOST_CHECK(auth_schemes.size() == 1);
    BOOST_CHECK(auth_schemes.front().is_ntlm());
    BOOST_CHECK(auth_schemes.front().get_challenge() == "challengeString=");

    // Negotiate with Challenge
    auth_headers = "neGotiate challengeString=";

    auth_schemes = websocketpp::http::proxy::auth_parser::parse_auth_schemes(auth_headers.begin(), auth_headers.end());

    BOOST_CHECK(auth_schemes.size() == 1);
    BOOST_CHECK(auth_schemes.front().is_negotiate());
    BOOST_CHECK(auth_schemes.front().get_challenge() == "challengeString=");

    // Valid Basic Auth + NTLM (mixed case)
    auth_headers = "baSic realm=\"some realm\",type=1,nTlm";

    auth_schemes = websocketpp::http::proxy::auth_parser::parse_auth_schemes(auth_headers.begin(), auth_headers.end());

    BOOST_CHECK(auth_schemes.size() == 2);
    BOOST_CHECK(auth_schemes[0].is_basic());
    BOOST_CHECK(auth_schemes[0].get_realm() == "some realm");
    BOOST_CHECK(auth_schemes[1].is_ntlm());
    BOOST_CHECK(auth_schemes[1].get_challenge().empty());

    auto auth_scheme = websocketpp::http::proxy::auth_parser::select_auth_scheme(auth_headers);

    BOOST_CHECK(auth_scheme.is_ntlm());
    BOOST_CHECK(auth_scheme.get_challenge().empty());

    // Digest + NTLM + Basic
    auth_headers = "Digest, NTLM, baSic realm=\"some realm\",type=1";

    auth_schemes = websocketpp::http::proxy::auth_parser::parse_auth_schemes(auth_headers.begin(), auth_headers.end());

    BOOST_CHECK(auth_schemes.size() == 3);
    BOOST_CHECK(auth_schemes[0].is_digest());
    BOOST_CHECK(auth_schemes[1].is_ntlm());
    BOOST_CHECK(auth_schemes[1].get_challenge().empty());
    BOOST_CHECK(auth_schemes[2].is_basic());
    BOOST_CHECK(auth_schemes[2].get_realm() == "some realm");

    auth_scheme = websocketpp::http::proxy::auth_parser::select_auth_scheme(auth_headers);

    BOOST_CHECK(auth_scheme.is_ntlm());
    BOOST_CHECK(auth_scheme.get_challenge().empty());

    // Digest + NTLM + Basic + Negotiate
    auth_headers = "Digest, NTLM, baSic realm=\"some realm\",type=1, negotiate";

    auth_schemes = websocketpp::http::proxy::auth_parser::parse_auth_schemes(auth_headers.begin(), auth_headers.end());

    BOOST_CHECK(auth_schemes.size() == 4);
    BOOST_CHECK(auth_schemes[0].is_digest());
    BOOST_CHECK(auth_schemes[1].is_ntlm());
    BOOST_CHECK(auth_schemes[1].get_challenge().empty());
    BOOST_CHECK(auth_schemes[2].is_basic());
    BOOST_CHECK(auth_schemes[2].get_realm() == "some realm");
    BOOST_CHECK(auth_schemes[3].is_negotiate());
    BOOST_CHECK(auth_schemes[3].get_challenge().empty());

    auth_scheme = websocketpp::http::proxy::auth_parser::select_auth_scheme(auth_headers);

    BOOST_CHECK(auth_scheme.is_negotiate());
    BOOST_CHECK(auth_scheme.get_challenge().empty());

    // Unknown Auth Scheme fails the parse for all schemes
    auth_headers = "Digest, NTLM, Basic realm=\"some realm\",type=1, NegotiateX";

    auth_schemes = websocketpp::http::proxy::auth_parser::parse_auth_schemes(auth_headers.begin(), auth_headers.end());

    BOOST_CHECK(auth_schemes.empty());

    // Empty Parameter value for a basic auth fails the parse
    auth_headers = "Digest, NTLM, Basic realm=\"some realm\",type=, Negotiate";

    auth_schemes = websocketpp::http::proxy::auth_parser::parse_auth_schemes(auth_headers.begin(), auth_headers.end());

    BOOST_CHECK(auth_schemes.empty());
}

class fake_security_context {
public:
    typedef websocketpp::lib::shared_ptr<fake_security_context> Ptr;
    typedef std::function<void(Ptr)> ReportContext;

    static ReportContext report_context;

    static fake_security_context::Ptr build(const std::string& proxyName, const std::string& authScheme) {
        auto context = websocketpp::lib::make_shared<fake_security_context>(proxyName, authScheme);

        if (report_context) {
            report_context(context);
        }

        return context;
    }

    fake_security_context(const std::string& proxyName, const std::string& authScheme) {
    }

    bool nextAuthToken(const std::string& challenge) {
        m_last_challenge = challenge;
        return m_auth_token.empty() ? false : true;
    }

    std::string getUpdatedToken() const {
        return m_auth_token;
    }

    std::string m_auth_token;
    std::string m_last_challenge;
};

fake_security_context::ReportContext fake_security_context::report_context;

BOOST_AUTO_TEST_CASE(proxy_authenticator_tests) {
    //
    // Setup interceptor to access the security_context object
    //
    std::string proxy_name("myProxy.com");

    fake_security_context::Ptr security_context;
    fake_security_context::report_context = [&security_context](fake_security_context::Ptr newContext) {
        security_context = newContext;
        newContext->m_auth_token = "Token1=";
    };

    // 
    // Test an typical NTLM multi step challenge auth flow
    //
    {
        security_context.reset();

        websocketpp::http::proxy::proxy_authenticator<fake_security_context> proxy_authenticator(proxy_name);

        BOOST_CHECK(!security_context);

        BOOST_CHECK(proxy_authenticator.get_auth_token().empty());

        std::string auth_headers = "NTLM challenge1=";

        BOOST_CHECK(proxy_authenticator.next_token(auth_headers));
        BOOST_CHECK(security_context);
        BOOST_CHECK(security_context->m_last_challenge == "challenge1=");
        BOOST_CHECK(proxy_authenticator.get_auth_token() == "NTLM Token1=");
        BOOST_CHECK(proxy_authenticator.get_authenticated_token().empty());

        security_context->m_auth_token = "Token2=";
        auth_headers = "NTLM challenge2=";

        BOOST_CHECK(proxy_authenticator.next_token(auth_headers));
        BOOST_CHECK(security_context->m_last_challenge == "challenge2=");
        BOOST_CHECK(proxy_authenticator.get_auth_token() == "NTLM Token2=");
        BOOST_CHECK(proxy_authenticator.get_authenticated_token().empty());

        proxy_authenticator.set_authenticated();
        BOOST_CHECK(proxy_authenticator.get_auth_token() == "NTLM Token2=");
        BOOST_CHECK(proxy_authenticator.get_authenticated_token() == "NTLM Token2=");
    }

    // 
    // Negotiate Flow (same as NTLM, but we're checking case insensitive)
    //
    {
        security_context.reset();

        websocketpp::http::proxy::proxy_authenticator<fake_security_context> proxy_authenticator(proxy_name);

        BOOST_CHECK(!security_context);

        BOOST_CHECK(proxy_authenticator.get_auth_token().empty());

        std::string auth_headers = "NeGoTiAtE challenge1=";

        BOOST_CHECK(proxy_authenticator.next_token(auth_headers));
        BOOST_CHECK(security_context);
        BOOST_CHECK(security_context->m_last_challenge == "challenge1=");
        BOOST_CHECK(proxy_authenticator.get_auth_token() == "NeGoTiAtE Token1=");
        BOOST_CHECK(proxy_authenticator.get_authenticated_token().empty());

        security_context->m_auth_token = "Token2=";
        auth_headers = "Negotiate challenge2=";

        BOOST_CHECK(proxy_authenticator.next_token(auth_headers));
        BOOST_CHECK(security_context->m_last_challenge == "challenge2=");
        BOOST_CHECK(proxy_authenticator.get_auth_token() == "NeGoTiAtE Token2=");
        BOOST_CHECK(proxy_authenticator.get_authenticated_token().empty());

        proxy_authenticator.set_authenticated();
        BOOST_CHECK(proxy_authenticator.get_auth_token() == "NeGoTiAtE Token2=");
        BOOST_CHECK(proxy_authenticator.get_authenticated_token() == "NeGoTiAtE Token2=");
    }
}

