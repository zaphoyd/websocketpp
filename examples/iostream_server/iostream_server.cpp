#include <websocketpp/config/core.hpp>

#include <websocketpp/server.hpp>

#include <iostream>
#include <fstream>

#include <unistd.h>

typedef websocketpp::server<websocketpp::config::core> server;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

// pull out the type of messages sent by our config
typedef server::message_ptr message_ptr;

// Define a callback to handle incoming messages
void on_message(server* s, websocketpp::connection_hdl hdl, message_ptr msg) {
    /*std::cout << "on_message called with hdl: " << hdl.lock().get() 
              << " and message: " << msg->get_payload()
              << std::endl;
*/
    try {
        s->send(hdl, msg->get_payload(), msg->get_opcode());
    } catch (const websocketpp::lib::error_code& e) {
  /*      std::cout << "Echo failed because: " << e  
                  << "(" << e.message() << ")" << std::endl;*/
    }
}

int main() {
    server s;
    std::ofstream log;
	
	try {        
        // Clear logging because we are using std out for data
        // TODO: fix when we can log to files
        s.set_error_channels(websocketpp::log::elevel::all);
        s.set_access_channels(websocketpp::log::alevel::all);
        
        
        log.open("output.log");
        
        s.get_alog().set_ostream(&log);
        s.get_elog().set_ostream(&log);
        
        // print all output to stdout
        s.register_ostream(&std::cout);
        
        // Register our message handler
        s.set_message_handler(bind(&on_message,&s,::_1,::_2));
        
        server::connection_ptr con = s.get_connection();
        
        con->start();
        
        //std::cin >> *con;
        
        //log << "ready done" << std::endl;
        
        
        char a;
        
        std::string temp;
        while(std::cin.get(a)) {
            con->readsome(&a,1);
            std::cout << a;
            std::cout.flush();
            s.get_alog().write(websocketpp::log::alevel::app, 
                	"Got input bytes: "+std::string(&a,1));
        }
        
        /*char buf[512];
        size_t bytes_read;
        size_t bytes_processed;
        
        while(std::cin) {
            bytes_read = std::cin.readsome(buf,512);
            
            if (bytes_read > 0) {
                std::cout << "read " << bytes_read << " bytes " 
                          << websocketpp::utility::to_hex(buf,bytes_read) 
                          << std::endl;
            }
            
            bytes_processed = 0;
            while (bytes_processed < bytes_read) {
                std::cout << "foo" << std::endl;
                bytes_processed += con->readsome(buf+bytes_processed,
                                                 bytes_read-bytes_processed);
                std::cout << "bar" << std::endl;
                sleep(1);
            }
            
            sleep(1);
        }*/
        std::cout << "end" << std::endl;
        
    } catch (const std::exception & e) {
        std::cout << e.what() << std::endl;
    } catch (websocketpp::lib::error_code e) {
        std::cout << e.message() << std::endl;
    } catch (...) {
        std::cout << "other exception" << std::endl;
    }
    log.close();
}



/*server test_server;
    server::connection_ptr con;
    
    test_server.set_message_handler(bind(&echo_func,&test_server,::_1,::_2));

    std::stringstream output;
	
	test_server.register_ostream(&output);
	
	con = test_server.get_connection();
	
	con->start();
	
	std::stringstream channel;
	
	channel << input;
	channel >> *con;
	
	return output.str();*/