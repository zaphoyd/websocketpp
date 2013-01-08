#include <websocketpp/config/asio.hpp>

#include <websocketpp/server.hpp>

#include <iostream>

typedef websocketpp::server<websocketpp::config::asio_tls> server;

/*class handler : public server::handler {
	
	std::string get_password() const {
        return "test";
    }
	
	websocketpp::lib::shared_ptr<boost::asio::ssl::context> on_tls_init() {
        // create a tls context, init, and return.
        websocketpp::lib::shared_ptr<boost::asio::ssl::context> context(new boost::asio::ssl::context(boost::asio::ssl::context::tlsv1));
        try {
            context->set_options(boost::asio::ssl::context::default_workarounds |
                                 boost::asio::ssl::context::no_sslv2 |
                                 boost::asio::ssl::context::single_dh_use);
            context->set_password_callback(boost::bind(&handler::get_password, this));
            context->use_certificate_chain_file("server.pem");
            context->use_private_key_file("server.pem", boost::asio::ssl::context::pem);
            //context->use_tmp_dh_file("../../src/ssl/dh512.pem");
        } catch (std::exception& e) {
            std::cout << e.what() << std::endl;
        }
        return context;
    }
	
	bool validate(connection_ptr con) {
		std::cout << "handler validate" << std::endl;
		return true;
	}
	
	void http(connection_ptr con) {
	    std::cout << "handler http" << std::endl;
	    con->set_status(websocketpp::http::status_code::OK);
	    con->set_body("foo");
	}
	
	void on_load(connection_ptr con, ptr old_handler) {
		std::cout << "handler on_load" << std::endl;
	}
	void on_unload(connection_ptr con, ptr new_handler) {
		std::cout << "handler on_unload" << std::endl;
	}
	
	void on_open(connection_ptr con) {
		std::cout << "handler on_open" << std::endl;
	}
	void on_fail(connection_ptr con) {
		std::cout << "handler on_fail" << std::endl;
	}
	
	void on_message(connection_ptr con, message_ptr msg) {
		std::cout << "handler on_message" << std::endl;
		std::cout << "Message was: " << msg->get_payload() << std::endl;
	}
	
	void on_close(connection_ptr con) {
		std::cout << "handler on_close" << std::endl;
	}
};*/

int main() {
	server echo_server;
	
	echo_server.init_asio();
	
	// Listen
	echo_server.listen(9002);
	
	// Start the server accept loop
	echo_server.start_accept();

	// Start the ASIO io_service run loop
	echo_server.run();

}
