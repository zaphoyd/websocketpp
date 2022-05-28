#include <websocketpp/config/asio_no_tls.hpp>

#include <websocketpp/server.hpp>

#include <iostream>

class persistent_config : public websocketpp::config::asio
{
public:
    static bool const enable_persistent_connections = true;
};

typedef websocketpp::server<persistent_config> server;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

const std::string html_file = "<html><head><title>WebSocketPP HTTP example</title><script src=\"test.js\"></script></head><body>Hello World!</body></html>";
const std::string js_file = "console.log(\"JavaScript loaded.\");";

void send_response(server::connection_ptr con, const std::string& mimeType, const std::string& content)
{
    con->replace_header("Content-Type", mimeType);
    con->set_body(content);
    con->set_status(websocketpp::http::status_code::ok);
}

void on_http_request(server* echo_server, websocketpp::connection_hdl hdl)
{
    server::connection_ptr con = echo_server->get_con_from_hdl(hdl);
    std::cout << "Received http request on hdl=" << std::hex << hdl.lock().get() << std::endl;

    const std::string& resource = con->get_uri()->get_resource();
    if (resource == "/") {
        send_response(con, "text/html", html_file);
    }
    else if (resource == "/test.js") {
        send_response(con, "application/javascript", js_file);
    }
    else {
        con->append_header("Connection", "close");
        con->set_status(websocketpp::http::status_code::not_found);
    }
}

int main() {
    // Create a server endpoint
    server echo_server;

    try {
        // Set logging settings
        echo_server.set_access_channels(websocketpp::log::alevel::all);
        echo_server.clear_access_channels(websocketpp::log::alevel::frame_payload);

        // Initialize Asio
        echo_server.init_asio();

        // Register our message handler
        echo_server.set_http_handler(bind(&on_http_request, &echo_server, ::_1));

        // Listen on port 9002
        echo_server.listen(9002);

        // Start the server accept loop
        echo_server.start_accept();

        // Start the ASIO io_service run loop
        echo_server.run();
    } catch (websocketpp::exception const & e) {
        std::cout << e.what() << std::endl;
    } catch (...) {
        std::cout << "other exception" << std::endl;
    }
}
