#include <websocketpp/config/asio_no_tls.hpp>

#include <websocketpp/server.hpp>

#include <iostream>

typedef websocketpp::server<websocketpp::config::asio> server;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

/* on_open insert connection_hdl into channel
 * on_close remove connection_hdl from channel
 * on_message queue send to all channels
 */

class broadcast_server {
public:
    broadcast_server() {
        // Initialize Asio Transport
        m_server.init_asio();
        
        // Register handler callbacks
        m_server.set_open_handler(bind(&on_open,this,::_1));
        m_server.set_close_handler(bind(&on_message,this,::_1));
        m_server.set_message_handler(bind(&on_message,this,::_1,::_2));
    }
    
    void run(uint16_t port) {
        // listen on specified port
        m_server.listen(port);
        
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
    
    void on_open(connection_hdl hdl) {
        boost::unique_lock<boost::mutex> lock(m_lock);
        m_connections.insert(hdl);
    }
    
    void on_close(connection_hdl hdl) {
        boost::unique_lock<boost::mutex> lock(m_lock);
        m_connections.remove(hdl);
    }
    
    void on_message(connection_hdl hdl, server::message_ptr msg) {
        // queue message up for sending by processing thread
        boost::unique_lock<boost::mutex> lock(m_lock);
        m_message_queue.push(msg);
        lock.unlock();
        m_cond.notify_one();
    }
    
    void process_messages() {
        while(1) {
            boost::unique_lock<boost::mutex> lock(m_lock);
            
            while(m_message_queue.empty()) {
                m_cond.wait(m_lock);
            }
            
            message_ptr msg = m_message_queue.front();
            m_message_queue.pop();
            
            lock.unlock();
            
            
        }
    }
private:
    server m_server;
    std::set<connection_hdl> m_connections;
    std::queue<server::message_ptr> m_message_queue;
    
    boost::mutex m_mutex;
    boost::condition_variable m_cond;
}

int main() {
	broadcast_server server;
	
	// Start a thread to run the processing loop
	boost::thread(bind(&broadcast_server::process_messages,&server));
	
	// Run the asio loop with the main thread
	server.run(9002);
}
