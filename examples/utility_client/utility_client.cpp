#include <websocketpp/config/asio_client.hpp>

#include <websocketpp/client.hpp>

#include <iostream>

typedef websocketpp::client<websocketpp::config::asio_tls_client> client;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind; 

// pull out the type of messages sent by our config
typedef websocketpp::config::asio_tls_client::message_type::ptr message_ptr;
typedef websocketpp::lib::shared_ptr<boost::asio::ssl::context> context_ptr;
typedef client::connection_ptr connection_ptr;

context_ptr on_tls_init(client* c, websocketpp::connection_hdl hdl) {
    context_ptr ctx(new boost::asio::ssl::context(boost::asio::ssl::context::tlsv1));

    try {
        ctx->set_options(boost::asio::ssl::context::default_workarounds |
                         boost::asio::ssl::context::no_sslv2 |
                         boost::asio::ssl::context::single_dh_use);
        //ctx->set_password_callback(bind(&get_password));
        //ctx->use_certificate_chain_file("server.pem");
        //ctx->use_private_key_file("server.pem", boost::asio::ssl::context::pem);
    } catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }
    return ctx;
}

void print_handler(client* c, websocketpp::connection_hdl hdl, message_ptr msg) {
    c->get_alog().write(websocketpp::log::alevel::app,msg->get_payload());
}

void write_on_open(client* c, websocketpp::connection_hdl hdl) {
    c->send(hdl, "ping", websocketpp::frame::opcode::text);
}

int main(int argc, char* argv[]) {
	// Create a server endpoint
    client endpoint;
	
	std::string uri = "wss://echo.websocket.org";
	
	if (argc == 2) {
	    uri = argv[1];
	}
	
	try {
        // We expect there to be a lot of errors, so suppress them
        endpoint.set_access_channels(websocketpp::log::alevel::all);
        endpoint.set_error_channels(websocketpp::log::elevel::all);
        
        // Initialize ASIO
        endpoint.init_asio();
        
        // Register our handlers
        endpoint.set_message_handler(bind(&print_handler,&endpoint,::_1,::_2));
        endpoint.set_open_handler(bind(&write_on_open,&endpoint,::_1));
        endpoint.set_tls_init_handler(bind(&on_tls_init,&endpoint,::_1));
        
        websocketpp::lib::error_code ec;
        client::connection_ptr con = endpoint.get_connection(uri, ec);
        
        //con->set_proxy("http://humupdates.uchicago.edu:8443");
        
        endpoint.connect(con);
	    
	    // Start the ASIO io_service run loop
        endpoint.run();
    } catch (const std::exception & e) {
        std::cout << e.what() << std::endl;
    } catch (websocketpp::lib::error_code e) {
        std::cout << e.message() << std::endl;
    } catch (...) {
        std::cout << "other exception" << std::endl;
    }
}
