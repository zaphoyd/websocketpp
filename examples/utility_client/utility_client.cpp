/**
 * This example is presently used as a scratch space. It may or may not be broken
 * at any given time.
 */

#include <websocketpp/config/asio_client.hpp>

#include <websocketpp/client.hpp>

#include <iostream>
#include <chrono>

typedef websocketpp::client<websocketpp::config::asio_tls_client> client;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

// pull out the type of messages sent by our config
typedef websocketpp::config::asio_tls_client::message_type::ptr message_ptr;
typedef websocketpp::lib::shared_ptr<boost::asio::ssl::context> context_ptr;
typedef client::connection_ptr connection_ptr;



class perftest {
public:
    typedef perftest type;
    typedef std::chrono::duration<int,std::micro> dur_type;

    perftest () {
        m_endpoint.set_access_channels(websocketpp::log::alevel::none);
        m_endpoint.set_error_channels(websocketpp::log::elevel::none);

        // Initialize ASIO
        m_endpoint.init_asio();

        // Register our handlers
        m_endpoint.set_tls_init_handler(bind(&type::on_tls_init,this,::_1));

        m_endpoint.set_tcp_pre_init_handler(bind(&type::on_tcp_pre_init,this,::_1));
        m_endpoint.set_tcp_post_init_handler(bind(&type::on_tcp_post_init,this,::_1));
        m_endpoint.set_socket_init_handler(bind(&type::on_socket_init,this,::_1));
        m_endpoint.set_message_handler(bind(&type::on_message,this,::_1,::_2));
        m_endpoint.set_open_handler(bind(&type::on_open,this,::_1));
        m_endpoint.set_close_handler(bind(&type::on_close,this,::_1));
    }

    void start(std::string uri) {
        websocketpp::lib::error_code ec;
        client::connection_ptr con = m_endpoint.get_connection(uri, ec);

        if (ec) {
        	m_endpoint.get_alog().write(websocketpp::log::alevel::app,ec.message());
        }

        //con->set_proxy("http://humupdates.uchicago.edu:8443");

        m_endpoint.connect(con);

	    // Start the ASIO io_service run loop
	    m_start = std::chrono::high_resolution_clock::now();
        m_endpoint.run();
    }

    void on_tcp_pre_init(websocketpp::connection_hdl hdl) {
        m_tcp_pre_init = std::chrono::high_resolution_clock::now();
    }
    void on_tcp_post_init(websocketpp::connection_hdl hdl) {
        m_tcp_post_init = std::chrono::high_resolution_clock::now();
    }
    void on_socket_init(websocketpp::connection_hdl hdl) {
        m_socket_init = std::chrono::high_resolution_clock::now();
    }

    context_ptr on_tls_init(websocketpp::connection_hdl hdl) {
        context_ptr ctx(new boost::asio::ssl::context(boost::asio::ssl::context::tlsv1));

        try {
            ctx->set_options(boost::asio::ssl::context::default_workarounds |
                             boost::asio::ssl::context::no_sslv2 |
                             boost::asio::ssl::context::single_dh_use);
        } catch (std::exception& e) {
            std::cout << e.what() << std::endl;
        }
        return ctx;
    }

    void on_open(websocketpp::connection_hdl hdl) {
        m_open = std::chrono::high_resolution_clock::now();

        client::connection_ptr con = m_endpoint.get_con_from_hdl(hdl);

        m_msg = con->get_message(websocketpp::frame::opcode::text,64);
        m_msg->append_payload(std::string(60,'*'));
        m_msg_count = 1;

        //m_message_stamps.reserve(1000);

        m_con_start = std::chrono::high_resolution_clock::now();

        m_endpoint.send(hdl, m_msg);
        //m_endpoint.send(hdl, "", websocketpp::frame::opcode::text);
    }
    void on_message(websocketpp::connection_hdl hdl, message_ptr msg) {
        if (m_msg_count == 1000) {
            m_message = std::chrono::high_resolution_clock::now();
            m_endpoint.close(hdl,websocketpp::close::status::going_away,"");
        } else {
            m_msg_count++;
            m_endpoint.send(hdl, m_msg);
        }
    }
    void on_close(websocketpp::connection_hdl hdl) {
        m_close = std::chrono::high_resolution_clock::now();



        std::cout << "Socket Init: " << std::chrono::duration_cast<dur_type>(m_socket_init-m_start).count() << std::endl;
        std::cout << "TCP Pre Init: " << std::chrono::duration_cast<dur_type>(m_tcp_pre_init-m_start).count() << std::endl;
        std::cout << "TCP Post Init: " << std::chrono::duration_cast<dur_type>(m_tcp_post_init-m_start).count() << std::endl;
        std::cout << "Open: " << std::chrono::duration_cast<dur_type>(m_open-m_start).count() << std::endl;
        std::cout << "Start: " << std::chrono::duration_cast<dur_type>(m_con_start-m_start).count() << std::endl;
        std::cout << "Message: " << std::chrono::duration_cast<dur_type>(m_message-m_start).count() << std::endl;
        std::cout << "Close: " << std::chrono::duration_cast<dur_type>(m_close-m_start).count() << std::endl;
        std::cout << std::endl;
        std::cout << "Message: " << std::chrono::duration_cast<dur_type>(m_message-m_con_start).count() << std::endl;
        std::cout << "Close: " << std::chrono::duration_cast<dur_type>(m_close-m_message).count() << std::endl;
    }
private:
    client m_endpoint;

    client::message_ptr m_msg;
    size_t m_msg_count;

    std::chrono::high_resolution_clock::time_point m_start;
    std::chrono::high_resolution_clock::time_point m_tcp_pre_init;
    std::chrono::high_resolution_clock::time_point m_tcp_post_init;
    std::chrono::high_resolution_clock::time_point m_socket_init;

    std::vector<std::chrono::high_resolution_clock::time_point> m_message_stamps;

    std::chrono::high_resolution_clock::time_point m_con_start;
    std::chrono::high_resolution_clock::time_point m_open;
    std::chrono::high_resolution_clock::time_point m_message;
    std::chrono::high_resolution_clock::time_point m_close;
};

int main(int argc, char* argv[]) {
	std::string uri = "wss://echo.websocket.org";

	if (argc == 2) {
	    uri = argv[1];
	}

	try {
        perftest endpoint;
        endpoint.start(uri);
    } catch (const std::exception & e) {
        std::cout << e.what() << std::endl;
    } catch (websocketpp::lib::error_code e) {
        std::cout << e.message() << std::endl;
    } catch (...) {
        std::cout << "other exception" << std::endl;
    }
}
