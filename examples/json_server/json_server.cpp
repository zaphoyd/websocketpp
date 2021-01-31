#include <boost/version.hpp>
#include <boost/config.hpp>

#if defined( BOOST_WINDOWS )
#pragma warning(push)
#pragma warning(disable : 4996 )
#endif
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#if defined( BOOST_WINDOWS )
#pragma warning(pop)
#endif

#include <iostream>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/exceptions.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/filesystem.hpp>

#include "protocol_messages.hpp"

typedef websocketpp::server<websocketpp::config::asio> server;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

// pull out the type of messages sent by our config
typedef server::message_ptr message_ptr;

std::string on_protocol_message(const msg_question_files_current_directory& received) {
    msg_response_files_current_directory response(received);

    using namespace std;
    using namespace boost::filesystem;
    path cp = current_path();

    directory_iterator end;
    for (directory_iterator it(cp); it != end; ++it) {
        if (!is_regular_file(it->status()))
            continue;
        string file = path(it->path()).make_preferred().string();
        if (file.length() > received.max_length) {
            // shorten path to max_length inserting "..." in middle
            auto midLeft = file.begin();
            advance(midLeft, file.length()/2);
            auto midRight = midLeft;
            advance(midLeft, received.max_length/2 - distance(file.begin(), midLeft) - 2);
            string left(file.begin(), midLeft);

            advance(midRight, distance(midRight, file.end()) - received.max_length/2 + 1);
            string right(midRight, file.end());

            file = left + "..." + right;
        }
        response.files.push_back(file);
    }

    return response.json();
}

std::string on_protocol_message(const msg_question_echo_timed& received) {
    msg_response_echo_timed send(received);

    using namespace std;
    using namespace boost::posix_time;
    using namespace boost::gregorian;
    ptime now = microsec_clock::universal_time();
    time_duration remaining = now - ptime(date(1970, Jan, 1));
    send.server_received = remaining.total_milliseconds();

    return send.json();
}

// Define a callback to handle incoming messages
void on_message(server* s, websocketpp::connection_hdl hdl, message_ptr msg) {
    std::cout << "Handle incoming message:" << msg->get_payload() << std::endl << "\topcode:" << msg->get_opcode() << std::endl;

    using namespace std;
    using namespace boost::property_tree;

    try { 

        ptree pt; {
            stringstream ss(msg->get_payload());
            read_json(ss, pt);
        }
        string answer;

        const unsigned char msgId = pt.get<unsigned char>("id");
        switch (msgId) {
        case question_echo_timed:
            answer = on_protocol_message(msg_question_echo_timed(pt));
            break;
        case question_files_current_directory:
            answer = on_protocol_message(msg_question_files_current_directory(pt));
            break;
        default:
            cerr << "unknown protocol message id:" << msgId << endl;
            exit(1);
        }

        s->send(hdl, answer, msg->get_opcode());
    }
    catch (ptree_error& err){
        cerr << "property tree exception:" << err.what() << endl;
    }
}

// WebSocket++ issue, is called on open session, should before
void on_create_socket(websocketpp::connection_hdl hdl, boost::asio::ip::tcp::socket& socket) {
    boost::system::error_code ec;
    std::cout << "socket created on port " << socket.local_endpoint(ec).port() << std::endl;
}

void on_open_session(websocketpp::connection_hdl hdl) {
    std::cout << "session opened" << std::endl;
}

void on_close_session(websocketpp::connection_hdl hdl) {
    std::cout << "session closed" << std::endl;
}

int main() {

    {
        boost::property_tree::ptree pt;
        pt.add("id", question_files_current_directory);
        pt.add("max_length", 30);

        msg_question_files_current_directory msg(pt);
        std::string str = on_protocol_message(msg);
        std::cout << str << std::endl;
    }

    // Create a server endpoint
    server testee_server;

    try {
        // spy some events
        testee_server.set_socket_init_handler(&on_create_socket);
        testee_server.set_tcp_post_init_handler(&on_open_session);
        testee_server.set_close_handler(&on_close_session);

        // Total silence
        testee_server.clear_access_channels(websocketpp::log::alevel::all);
        testee_server.clear_error_channels(websocketpp::log::alevel::all);

        // Initialize ASIO
        testee_server.init_asio();

        // Register our message handler
        testee_server.set_message_handler(bind(&on_message, &testee_server, ::_1, ::_2));

        // Listen on port 9002
        testee_server.listen(9002);

        // Start the server accept loop
        testee_server.start_accept();

        // Start the ASIO io_service run loop
        testee_server.run();
    }
    catch (const std::exception & e) {
        std::cout << e.what() << std::endl;
    }
    catch (websocketpp::lib::error_code e) {
        std::cout << e.message() << std::endl;
    }
    catch (...) {
        std::cout << "other exception" << std::endl;
    }
}
