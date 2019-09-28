
#ifndef WEBSOCKETPP_TRANSPORT_ASIO_SOCKS5_HPP
#define WEBSOCKETPP_TRANSPORT_ASIO_SOCKS5_HPP

#include <websocketpp/uri.hpp>
#include <websocketpp/common/network.hpp>

namespace websocketpp {
namespace transport {
namespace asio {

enum negotiation_phase {
    method_selection,
    basic_authentication,
    connect,
};

class socks5_request {
public:

    const unsigned char version = 0x5;
    const unsigned char connect = 0x1;
    const unsigned char domain_name = 0x3;
    const unsigned char reserved = 0x0;

    void set_basic_auth(std::string const & username, std::string const & password) {
        m_username = username;
        m_password = password;
    }

    void set_uri(uri_ptr uri) {
        m_uri = uri;
    }

    const std::vector<unsigned char>& get_method_selection_buf() {
        m_buf.clear();
        m_buf.reserve(5);
        m_buf.push_back(version);
        m_buf.push_back(1); // num methods. corrected later.
        m_buf.push_back(0); // no authentication
        if (!m_username.empty() && !m_password.empty()) {
            m_buf.push_back(2); // basic auth
            ++m_buf[1];
        }
        return m_buf;
    }

    const std::vector<unsigned char>& get_basic_authentication_buf() {
        m_buf.clear();
        const unsigned char username_siz = static_cast<unsigned char>(m_username.size());
        const unsigned char password_siz = static_cast<unsigned char>(m_password.size());
        m_buf.resize(3 + username_siz + password_siz);
        uint16_t len = 0;
        m_buf[len++] = 1; // version
        m_buf[len++] = username_siz;
        std::copy(m_username.begin(), m_username.begin() + username_siz, m_buf.begin() + len);
        len += username_siz;
        m_buf[len++] = password_siz;
        std::copy(m_password.begin(), m_password.begin() + password_siz, m_buf.begin() + len);
        return m_buf;
    }

    const std::vector<unsigned char>& get_connect_buf() {
        m_buf.clear();

        if (!m_uri) {
            return m_buf;
        }

        const std::string host = m_uri->get_host();
        const uint16_t port = m_uri->get_port();

        m_buf.resize(5 + host.size() + sizeof(uint16_t));
        m_buf[0] = version;
        m_buf[1] = connect;
        m_buf[2] = reserved;
        m_buf[3] = domain_name;
        m_buf[4] = static_cast<unsigned char>(host.size());
        std::copy(host.begin(), host.end(), m_buf.begin() + 5);
        const uint16_t n = htons(port);
        unsigned char* p = m_buf.data() + m_buf.size() - sizeof(uint16_t);
        memcpy(p, &n, sizeof(uint16_t));
        return m_buf;
    }

private:
    std::vector<unsigned char> m_buf;
    std::string m_username;
    std::string m_password;
    uri_ptr m_uri;
};

class socks5_reply {
public:

    const unsigned char version = 0x5;

    std::vector<lib::asio::mutable_buffer> get_method_selection_buf() {
        std::vector<lib::asio::mutable_buffer> bufs;
        bufs.push_back(lib::asio::buffer(&m_version, 1));
        bufs.push_back(lib::asio::buffer(&m_method, 1));
        return bufs;
    }

    std::vector<lib::asio::mutable_buffer> get_basic_authentication_buf() {
        std::vector<lib::asio::mutable_buffer> bufs;
        bufs.push_back(lib::asio::buffer(&m_version, 1));
        bufs.push_back(lib::asio::buffer(&m_status, 1));
        return bufs;
    }

    std::vector<lib::asio::mutable_buffer> get_connect_buf() {
        std::vector<lib::asio::mutable_buffer> bufs;
        bufs.push_back(lib::asio::buffer(&m_version, 1));
        bufs.push_back(lib::asio::buffer(&m_reply, 1));
        bufs.push_back(lib::asio::buffer(&m_reserved, 1));
        bufs.push_back(lib::asio::buffer(&m_address_type, 1));
        m_host_port.resize(16 + sizeof(uint16_t));
        bufs.push_back(lib::asio::buffer(m_host_port));
        return bufs;
    }

    size_t get_min_connect_size() const {
        return 8 + sizeof(uint16_t);
    }

    unsigned char get_version() const {
        return m_version;
    }

    unsigned char get_method() const {
        return m_method;
    }

    unsigned char get_reply() const {
        return m_reply;
    }

    unsigned char get_status() const {
        return m_status;
    }

private:
    unsigned char m_version;
    unsigned char m_method;
    unsigned char m_reply;
    unsigned char m_reserved;
    unsigned char m_address_type;
    unsigned char m_status;
    std::vector<unsigned char> m_host_port;
};

} // namespace asio
} // namespace transport
} // namespace websocketpp

#endif // WEBSOCKETPP_TRANSPORT_ASIO_SOCKS5_HPP
