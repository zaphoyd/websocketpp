#include "/home/nadkalpur/Documents/websocketpp/websocketpp/asio_client.hpp" // Include the WebSocket++ ASIO client header
#include <websocketpp/client.hpp> // Include the WebSocket++ client header
#include <iostream> // Include the standard I/O library

// Define the WebSocket++ client type using the ASIO TLS client configuration
using client = websocketpp::client<websocketpp::config::asio_tls_client>;

// Define the shared pointer type for SSL context
using context_ptr = websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context>;

// Import placeholders for binding functions
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

// Import the bind function for binding handlers
using websocketpp::lib::bind;

// Use the std::string_literals namespace for string literals
using namespace std::string_literals;

// Handler for when the WebSocket connection is opened
void on_open(websocketpp::connection_hdl hdl, client* c) {
    std::cout << "WebSocket connection opened!" << std::endl;

    websocketpp::lib::error_code ec; // Error code object
    client::connection_ptr con = c->get_con_from_hdl(hdl, ec); // Get the connection pointer from the handle

    if (ec) { // Check if there was an error
        std::cout << "Failed to get connection pointer: " << ec.message() << std::endl;
        return;
    }

    // Payload to send to the WebSocket server
    std::string payload = "{\"userKey\":\"wsO10gpDdcV2gIBLBrnw\", \"symbol\":\"EURUSD,GBPUSD\"}";
    c->send(con, payload, websocketpp::frame::opcode::text); // Send the payload
}

// Handler for when a message is received
void on_message(websocketpp::connection_hdl, client::message_ptr msg) {
    std::cout << "Currency Pairs: " << msg->get_payload() << std::endl; // Print the received message
}

// Handler for when the WebSocket connection fails
void on_fail(websocketpp::connection_hdl hdl) {
    std::cout << "WebSocket connection failed!" << std::endl;
}

// Handler for when the WebSocket connection is closed
void on_close(websocketpp::connection_hdl hdl) {
    std::cout << "WebSocket connection closed!" << std::endl;
}

// Handler for initializing the TLS context
context_ptr on_tls_init(const char* hostname, websocketpp::connection_hdl) {
    // Create a shared pointer for the SSL context
    context_ptr ctx = websocketpp::lib::make_shared<boost::asio::ssl::context>(
        boost::asio::ssl::context::tlsv12); // Use TLS v1.2

    try {
        using boost::asio::ssl::context; // Simplify the context namespace usage
        // Set SSL context options
        ctx->set_options(context::default_workarounds |
                         context::no_sslv2 |
                         context::no_sslv3 |
                         context::single_dh_use);
    } catch (std::exception& e) { // Catch any exceptions
        std::cout << "TLS Initialization Error: " << e.what() << std::endl;
    }

    return ctx; // Return the SSL context
}

// Main function
int main(int argc, char* argv[]) {
    client c; // Create a WebSocket++ client object

    // Define the hostname and URI for the WebSocket server
    std::string hostname = "marketdata.tradermade.com/feedadv";
    std::string uri = "wss://" + hostname;

    try {
        // Set logging options for the client
        c.set_access_channels(websocketpp::log::alevel::all); // Enable all access logs
        c.clear_access_channels(websocketpp::log::alevel::frame_payload); // Disable frame payload logs
        c.set_error_channels(websocketpp::log::elevel::all); // Enable all error logs

        c.init_asio(); // Initialize the ASIO transport

        // Set the handlers for various WebSocket events
        c.set_message_handler(&on_message); // Set the message handler
        c.set_tls_init_handler(bind(&on_tls_init, hostname.c_str(), ::_1)); // Set the TLS initialization handler
        c.set_open_handler(bind(&on_open, ::_1, &c)); // Set the open handler
        c.set_fail_handler(bind(&on_fail, ::_1)); // Set the fail handler
        c.set_close_handler(bind(&on_close, ::_1)); // Set the close handler

        websocketpp::lib::error_code ec; // Error code object
        client::connection_ptr con = c.get_connection(uri, ec); // Create a connection to the server

        if (ec) { // Check if there was an error
            std::cout << "Could not create connection because: " << ec.message();
            return 0;
        }

        c.connect(con); // Connect to the server
        c.run(); // Start the ASIO event loop
    } catch (websocketpp::exception const& e) { // Catch any WebSocket++ exceptions
        std::cout << "WebSocket Exception: " << e.what() << std::endl;
    }
}
