#include <websocketpp/config/asio_no_tls.hpp>

#include <websocketpp/server.hpp>

#include <iostream>

/*#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>*/
#include <websocketpp/common/thread.hpp>

typedef websocketpp::server<websocketpp::config::asio> server;

using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

using websocketpp::lib::thread;
using websocketpp::lib::mutex;
using websocketpp::lib::unique_lock;
using websocketpp::lib::condition_variable;

/* on_open insert connection_hdl into channel
 * on_close remove connection_hdl from channel
 * on_message queue send to all channels
 */

enum action_type {
    SUBSCRIBE,
    UNSUBSCRIBE,
    MESSAGE
};

struct action {
    action(action_type t, connection_hdl h) : type(t), hdl(h) {}
    action(action_type t, server::message_ptr m) : type(t), msg(m) {}
    
    action_type type;
    websocketpp::connection_hdl hdl;
    server::message_ptr msg;
};
 
class broadcast_server {
public:
    broadcast_server() {
        // Initialize Asio Transport
        m_server.init_asio();
        
        // Register handler callbacks
        m_server.set_open_handler(bind(&broadcast_server::on_open,this,::_1));
        m_server.set_close_handler(bind(&broadcast_server::on_close,this,::_1));
        m_server.set_message_handler(bind(&broadcast_server::on_message,this,::_1,::_2));
    }
    
    void run(uint16_t port) {
        // listen on specified port
        m_server.listen(port);
        
        // Start the server accept loop
	    m_server.start_accept();
	    
	    // Start the ASIO io_service run loop
        try {
            m_server.run();
        } catch (const std::exception & e) {
            std::cout << e.what() << std::endl;
        } catch (websocketpp::lib::error_code e) {
            std::cout << e.message() << std::endl;
        } catch (...) {
            std::cout << "other exception" << std::endl;
        }
    }
    
    void on_open(connection_hdl hdl) {
        unique_lock<mutex> lock(m_action_lock);
        //std::cout << "on_open" << std::endl;
        m_actions.push(action(SUBSCRIBE,hdl));
        lock.unlock();
        m_action_cond.notify_one();
    }
    
    void on_close(connection_hdl hdl) {
        unique_lock<mutex> lock(m_action_lock);
        //std::cout << "on_close" << std::endl;
        m_actions.push(action(UNSUBSCRIBE,hdl));
        lock.unlock();
        m_action_cond.notify_one();
    }
    
    void on_message(connection_hdl hdl, server::message_ptr msg) {
        // queue message up for sending by processing thread
        unique_lock<mutex> lock(m_action_lock);
        //std::cout << "on_message" << std::endl;
        m_actions.push(action(MESSAGE,msg));
        lock.unlock();
        m_action_cond.notify_one();
    }
    
    void process_messages() {
        while(1) {
            unique_lock<mutex> lock(m_action_lock);
            
            while(m_actions.empty()) {
                m_action_cond.wait(lock);
            }
            
            action a = m_actions.front();
            m_actions.pop();
            
            lock.unlock();
            
            if (a.type == SUBSCRIBE) {
                unique_lock<mutex> lock(m_connection_lock);
                m_connections.insert(a.hdl);
            } else if (a.type == UNSUBSCRIBE) {
                unique_lock<mutex> lock(m_connection_lock);
                m_connections.erase(a.hdl);
            } else if (a.type == MESSAGE) {
                unique_lock<mutex> lock(m_connection_lock);
                
                con_list::iterator it;
                for (it = m_connections.begin(); it != m_connections.end(); ++it) {
                    m_server.send(*it,a.msg);
                }
            } else {
                // undefined.
            }
        }
    }
private:
    typedef std::set<connection_hdl,std::owner_less<connection_hdl>> con_list;
    
    server m_server;
    con_list m_connections;
    std::queue<action> m_actions;
    
    mutex m_action_lock;
    mutex m_connection_lock;
    condition_variable m_action_cond;
};

int main() {
	try {
	broadcast_server server;
	
	// Start a thread to run the processing loop
	thread t(bind(&broadcast_server::process_messages,&server));
	
	// Run the asio loop with the main thread
	server.run(9002);
	
	t.join();
	
	} catch (std::exception & e) {
	    std::cout << e.what() << std::endl;
	}
}
