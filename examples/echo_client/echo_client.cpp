#include <websocketpp/config/asio_no_tls_client.hpp>

#include <websocketpp/client.hpp>

#include <iostream>

typedef websocketpp::client<websocketpp::config::asio_client> client;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

// pull out the type of messages sent by our config
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;


// Define a callback to handle incoming messages
void on_open(client* c, websocketpp::connection_hdl hdl) {
    std::cout << "on_open called for " << hdl.lock().get() << std::endl;
    try {
        c->send(hdl, "Foo", websocketpp::frame::opcode::text);
    } catch (const websocketpp::lib::error_code& e) {
        std::cout << "Send failed because: " << e  
                  << "(" << e.message() << ")" << std::endl;
    }
}

void on_message(client* c, websocketpp::connection_hdl hdl, message_ptr msg) {
    std::cout << "on_message called with hdl: " << hdl.lock().get() 
              << " and message: " << msg->get_payload()
              << std::endl;

    /*try {
        c->send(hdl, msg->get_payload(), msg->get_opcode());
    } catch (const websocketpp::lib::error_code& e) {
        std::cout << "Echo failed because: " << e  
                  << "(" << e.message() << ")" << std::endl;
    }*/
}

int main() {
	// Create a server endpoint
    client echo_client;
	
	try {
        // Set logging settings
        echo_client.set_access_channels(websocketpp::log::alevel::all);
        echo_client.clear_access_channels(websocketpp::log::alevel::frame_payload);
        
        // Initialize ASIO
        echo_client.init_asio();
        
        // Register our handlers
        echo_client.set_message_handler(bind(&on_message,&echo_client,::_1,::_2));
        echo_client.set_open_handler(bind(&on_open,&echo_client,::_1));
        
        websocketpp::lib::error_code ec;
        client::connection_ptr con = echo_client.get_connection("ws://localhost:9002/", ec);
        echo_client.connect(con);
	    
	    // Start the ASIO io_service run loop
        echo_client.run();
    } catch (const std::exception & e) {
        std::cout << e.what() << std::endl;
    } catch (websocketpp::lib::error_code e) {
        std::cout << e.message() << std::endl;
    } catch (...) {
        std::cout << "other exception" << std::endl;
    }
}
