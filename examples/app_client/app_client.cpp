#include <websocketpp/common/thread.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include <iostream>
#include <map>
#include <queue>
#include <string>

typedef websocketpp::client<websocketpp::config::asio_client> client;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

// pull out the type of messages sent by our config
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

int case_count = 0;

void on_message(client* c, websocketpp::connection_hdl hdl, message_ptr msg) {
    client::connection_ptr con = c->get_con_from_hdl(hdl);

    if (con->get_resource() == "/getCaseCount") {
        std::cout << "Detected " << msg->get_payload() << " test cases."
                  << std::endl;
        case_count = atoi(msg->get_payload().c_str());
    } else {
        c->send(hdl, msg->get_payload(), msg->get_opcode());
    }
}

class app_connection {
public:
    typedef websocketpp::lib::shared_ptr<app_connection> ptr;

    app_connection(size_t id, websocketpp::connection_hdl hdl)
      : m_id(id)
      , m_hdl(hdl) {}

    void on_message(websocketpp::connection_hdl, message_ptr msg) {
        m_messages.push_back(msg->get_payload());
    }
    void on_fail(websocketpp::connection_hdl) {

    }
    void on_open(websocketpp::connection_hdl) {

    }
    void on_close(websocketpp::connection_hdl) {

    }

    bool print_new_messages() {
        websocketpp::lib::lock_guard<websocketpp::lib::mutex> guard(m_mutex);

        if (m_messages.empty()) {
            return false;
        }
        
        while(!m_messages.empty()) {
            std::cout << m_messages.front() << std::endl;
            m_messages.pop_front();
        }
    }

    websocketpp::connection_hdl get_hdl() const {
        return m_hdl;
    }
private:
    websocketpp::lib::mutex m_mutex;
    size_t m_id;
    // list of messages
    std::queue<std::string> m_messages;
    websocketpp::connection_hdl m_hdl;
};

class continuous_client_manager {
public:
    continuous_client_manager() : next_id(0) {
        // clear all error/access channels
        m_endpoint.clear_access_channels(websocketpp::log::alevel::all);
        m_endpoint.clear_error_channels(websocketpp::log::elevel::all);
        
        // Initialize the endpoint
        m_endpoint.init_asio();

        // Mark this endpoint as perpetual. Perpetual endpoints will not exit
        // even if there are no connections.
        m_endpoint.start_perpetual();

        // Start a background thread and run the endpoint in that thread
        m_thread.reset(new thread_type(&client::run, &m_endpoint));
    }

    app_connection::ptr connect(std::string const & uri) {
        websocketpp::lib::error_code ec;

        // connect to this address
        client::connection_ptr con = m_endpoint.get_connection(uri,ec);
        if (ec) {
            return app_connection::ptr();
        }
       
        
        app_connection::ptr app(new app_connection(next_id++,con->get_handle()));

        con->set_open_handler(bind(&app_connection::on_open,*app));
        con->set_fail_handler(bind(&app_connection::on_fail,*app));
        con->set_message_handler(bind(&app_connection::on_message,*app));
        con->set_close_handler(bind(&app_connection::on_close,*app));

        m_endpoint.connect(con);

        return app;
    }

    void close(websocketpp::connection_hdl hdl) {
        
    }

    void shutdown() {
        // for each connection call close

        // Unflag the endpoint as perpetual. This will instruct it to stop once
        // all connections are finished.
        m_endpoint.stop_perpetual();

        // Block until everything is done
        m_thread->join();
    }
private:
    typedef websocketpp::lib::thread thread_type;
    typedef websocketpp::lib::shared_ptr<thread_type> thread_ptr;
    
    size_t next_id;

    std::map<connection_hdl> m_connections;
    client m_endpoint;
    thread_ptr m_thread;
};

int main(int argc, char* argv[]) {
    continuous_client_manager client;

    std::string command;
    bool done = false;

    while (!done) {
        std::getline(std::cin, command);

        if (command == "quit") {
            done = true;
        } else if (command == "list") {
            
        } else if (command.substr(0,7) == "connect") {
            
        } else if (command.substr(0,5) == "close") {
            
        } else if (command.substr(0,8) == "messages") {
            
        } else {
            std::cout << "Invalid Command" << std::endl;
        }
    }

    // close connections and stop the endpoint
    client.shutdown();
}
