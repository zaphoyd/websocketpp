#include <websocketpp/config/asio_no_tls.hpp>

#include <websocketpp/server.hpp>

#include <iostream>

typedef websocketpp::server<websocketpp::config::asio> server;

class handler : public server::handler {
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
		//std::cout << "Message was: " << msg->get_payload() << std::endl;
        try {
           con->send(msg->get_payload(),msg->get_opcode());
        } catch (const websocketpp::lib::error_code& e) {
            std::cout << "Echo failed because: " << e  << "(" << e.message() << ")" << std::endl;
        }
	}

    bool on_ping(connection_ptr con, const std::string & msg) {
        std::cout << "handler on_ping" << std::endl;
        return true;
    }
	
    void on_pong(connection_ptr con, const std::string & msg) {
        std::cout << "handler on_pong" << std::endl;
    }
	
    void on_pong_timeout(connection_ptr con, const std::string & msg) {
        std::cout << "handler on_pong_timeout" << std::endl;
    }
	
	void on_close(connection_ptr con) {
		std::cout << "handler on_close" << std::endl;
	}

    void on_interrupt(connection_ptr con) {
		std::cout << "handler on_interrupt" << std::endl;
    }
};

class test_handler {
public:
    test_handler(server& s): m_server(s) {}

    void on_open(websocketpp::connection_hdl hdl) {
        std::cout << "on_open called with hdl: " << hdl.lock().get() << std::endl;
        m_server.interrupt(hdl);
    }
private:
    server& m_server;
};

void on_open(server* s, websocketpp::connection_hdl hdl) {
    std::cout << "on_open called with hdl: " << hdl.lock().get() << std::endl;
    s->interrupt(hdl);
}

void on_interrupt(server* s, websocketpp::connection_hdl hdl) {
    std::cout << "on_interrupt called with hdl: " << hdl.lock().get() << std::endl;
}

void on_tcp_init(websocketpp::connection_hdl hdl) {
    std::cout << "on_tcp_init called with hdl: " << hdl.lock().get() << std::endl;
}


using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

// pull out the type of messages sent by our config
typedef websocketpp::config::asio::message_type::ptr message_ptr;

void on_message(server* s,websocketpp::connection_hdl hdl,message_ptr msg) {
    std::cout << "on_message called with hdl: " << hdl.lock().get() 
              << " and message: " << msg->get_payload()
              << std::endl;
    //s->get_con_from_hdl(hdl)->send(msg->get_payload(),msg->get_opcode());
}

int main() {
    server::handler::ptr h(new handler());
	
	server echo_server(h);
	
	echo_server.init_asio();
	
    test_handler t(echo_server);

    echo_server.set_open_handler(bind(&on_open,&echo_server,::_1));
    echo_server.set_interrupt_handler(bind(&on_interrupt,&echo_server,::_1));
    echo_server.set_tcp_init_handler(&on_tcp_init);
    echo_server.set_message_handler(bind(&on_message,&echo_server,::_1,::_2));

	// Listen
	echo_server.listen(9002);
	
	// Start the server accept loop
	echo_server.start_accept();

	// Start the ASIO io_service run loop
	try {
        echo_server.run();
    } catch (const std::exception & e) {
        std::cout << e.what() << std::endl;
    } catch (websocketpp::lib::error_code e) {
        std::cout << e.message() << std::endl;
    } catch (...) {
        std::cout << "other exception" << std::endl;
    }
}
