#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include <websocketpp/common/thread.hpp>
#include <websocketpp/common/memory.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>

typedef websocketpp::client<websocketpp::config::asio_client> client;

class connection_metadata {
public:
    connection_metadata(int id, websocketpp::connection_hdl hdl,std::string uri)
      : m_id(id)
      , m_hdl(hdl)
      , m_status("Connecting")
      , m_uri(uri)
      , m_server("N/A")
    {}
    
    connection_metadata()
      : m_id(-1) {}
    
    void on_open(client * c, websocketpp::connection_hdl hdl) {
        m_status = "Open";
        client::connection_ptr con = c->get_con_from_hdl(hdl);
        m_server = con->get_response_header("Server");
    }
    
    void on_fail(client * c, websocketpp::connection_hdl hdl) {
        m_status = "Failed";
        client::connection_ptr con = c->get_con_from_hdl(hdl);
        m_error_reason = con->get_ec().message();
    }
    
    void on_close(client * c, websocketpp::connection_hdl hdl) {
        m_status = "Closed";
        client::connection_ptr con = c->get_con_from_hdl(hdl);
        std::stringstream s;
        s << "close code: " << con->get_remote_close_code() << ", close reason: " << con->get_remote_close_reason();
        m_error_reason = s.str();
    }
    
    void set_error(std::string const & err) {
        m_error_reason = err;
        m_status = "Error";
    }
    
    int get_id() const {
        return m_id;
    }
    
    websocketpp::connection_hdl get_hdl() const {
        return m_hdl;
    }
    
    friend std::ostream & operator<< (std::ostream & out, connection_metadata const & data);
private:
    int m_id;
    websocketpp::connection_hdl m_hdl;
    std::string m_status;
    std::string m_uri;
    std::string m_server;
    std::string m_error_reason;
};

std::ostream & operator<< (std::ostream & out, connection_metadata const & data) {
    if (data.m_id == -1) {
        out << "> Invalid connection";
    } else {
        out << "> URI: " << data.m_uri << "\n"
            << "> Status: " << data.m_status << "\n"
            << "> Remote Server: " << data.m_server << "\n"
            << "> Error/close reason: " << data.m_error_reason;
    }
    
    return out;
}

class websocket_endpoint {
public:
    websocket_endpoint () : m_next_id(0) {
        m_endpoint.clear_access_channels(websocketpp::log::alevel::all);
        m_endpoint.clear_error_channels(websocketpp::log::elevel::all);
        
        m_endpoint.set_access_channels(websocketpp::log::alevel::app);

        m_endpoint.init_asio();
        m_endpoint.start_perpetual();

        m_thread.reset(new websocketpp::lib::thread(&client::run, &m_endpoint));
    }
    
    ~websocket_endpoint() {
        m_endpoint.stop_perpetual();
        
        for (con_list::const_iterator it = m_connection_list.begin(); it != m_connection_list.end(); ++it) {
            websocketpp::lib::error_code ec;
            m_endpoint.close(it->second.get_hdl(), websocketpp::close::status::going_away, "", ec);
            if (ec) {
                std::cout << "> Error closing connection " << it->second.get_id() << std::endl;
            }
        }
        
        m_thread->join();
    }
    
    // copy constructors
    
    int connect(std::string const & uri) {
        websocketpp::lib::error_code ec;
        int new_id = m_next_id++;

        client::connection_ptr con = m_endpoint.get_connection(uri, ec);
        
        if (ec) {
            m_connection_list[new_id] = connection_metadata(new_id, websocketpp::connection_hdl(), uri);
            m_connection_list[new_id].set_error("Connect initialization error: "+ec.message());
            std::cout << "> Connect initialization error: " << ec.message() << std::endl;
        } else {
            m_connection_list[new_id] = connection_metadata(new_id, con->get_handle(), uri);
            
            using websocketpp::lib::placeholders::_1;
            using websocketpp::lib::bind;
            con->set_open_handler(bind(&connection_metadata::on_open,&m_connection_list[new_id],&m_endpoint,::_1));
            con->set_fail_handler(bind(&connection_metadata::on_fail,&m_connection_list[new_id],&m_endpoint,::_1));
            con->set_close_handler(bind(&connection_metadata::on_close,&m_connection_list[new_id],&m_endpoint,::_1));
            
            m_endpoint.connect(con);
        }   
        
        return new_id;
    }
    
    void close(int id, websocketpp::close::status::value code) {
        websocketpp::lib::error_code ec;
        
        con_list::iterator metadata_it = m_connection_list.find(id);
        if (metadata_it == m_connection_list.end()) {
            std::cout << "> No connection found with id " << id << std::endl;
            return;
        }
        
        m_endpoint.close(metadata_it->second.get_hdl(),code, "", ec);
        if (ec) {
            std::cout << "> Error initiating close: " << ec.message() << std::endl;
        }
    }
    
    connection_metadata get_metadata(int id) const {
        con_list::const_iterator metadata_it = m_connection_list.find(id);
        if (metadata_it == m_connection_list.end()) {
            return connection_metadata();
        } else {
            return metadata_it->second;
        }
    }
private:
    typedef std::map<int,connection_metadata> con_list;
    
    client m_endpoint;
    websocketpp::lib::shared_ptr<websocketpp::lib::thread> m_thread;
    
    con_list m_connection_list;
    int m_next_id;
};

int main() {
    bool done = false;
    std::string input;
    websocket_endpoint endpoint;

    while (!done) {
        std::cout << "Enter Command: ";
        std::getline(std::cin, input);

        if (input == "quit") {
            done = true;
        } else if (input == "help") {
            std::cout 
                << "\nCommand List:\n"
                << "connect <ws uri>\n"
                << "close <connection id>\n"
                << "show <connection id>\n"
                << "help: Display this help text\n"
                << "quit: Exit the program\n"
                << std::endl;
        } else if (input.substr(0,7) == "connect") {
            int id = endpoint.connect(input.substr(8));
            std::cout << "> Created connection with id " << id << std::endl;
        } else if (input.substr(0,5) == "close") {
            int id = atoi(input.substr(6).c_str());
            
            endpoint.close(id, websocketpp::close::status::normal);
        } else if (input.substr(0,4) == "show") {
            int id = atoi(input.substr(5).c_str());
            
            std::cout << endpoint.get_metadata(id) << std::endl;
        } else {
            std::cout << "> Unrecognized Command" << std::endl;
        }
    }

    return 0;
}

/*

clang++ -std=c++11 -stdlib=libc++ -I/Users/zaphoyd/software/websocketpp/ -I/Users/zaphoyd/software/boost_1_55_0/ -D_WEBSOCKETPP_CPP11_STL_ step4.cpp /Users/zaphoyd/software/boost_1_55_0/stage/lib/libboost_system.a

clang++ -I/Users/zaphoyd/software/websocketpp/ -I/Users/zaphoyd/software/boost_1_55_0/ step4.cpp /Users/zaphoyd/software/boost_1_55_0/stage/lib/libboost_system.a /Users/zaphoyd/software/boost_1_55_0/stage/lib/libboost_thread.a /Users/zaphoyd/software/boost_1_55_0/stage/lib/libboost_random.a
*/