#include <boost/config.hpp>
#include <boost/property_tree/json_parser.hpp>

enum message_identification {
    
    question_echo_timed =               1,
    response_echo_timed,                // 2
    
    question_files_current_directory,   // 3
    response_files_current_directory    // 4
};

struct message_protocol {

    const message_identification id;

    message_protocol(message_identification _id)
        : id(_id) {
    }

    message_protocol(message_identification _id, boost::property_tree::ptree& pt)
        : id(_id) {
        BOOST_STATIC_ASSERT(sizeof(id) == sizeof(int32_t));
        BOOST_ASSERT(pt.get<int32_t>("id") == id);
    }

    void json(boost::property_tree::ptree& pt) const {
        pt.add("id", id);
    }

    std::string json() const {
        boost::property_tree::ptree pt;
        json(pt);
        std::stringstream ss;
        write_json(ss, pt);
        return ss.str();
    }
};

struct msg_question_echo_timed : message_protocol {

    int64_t client_sent; // number of milliseconds since midnight January 1, 1970
    std::string message;

    // unjsonify is done in constructor
    msg_question_echo_timed(boost::property_tree::ptree& pt)
        : message_protocol(question_echo_timed, pt)
        , client_sent(pt.get<int64_t>("client_sent"))
        , message(pt.get<std::string>("message")) {
    }
};

struct msg_response_echo_timed : message_protocol {

    msg_question_echo_timed client_to_server;
    int64_t server_received; // number of milliseconds since midnight January 1, 1970

    msg_response_echo_timed(const msg_question_echo_timed&  msg)
        : message_protocol(response_echo_timed)
        , client_to_server(msg) {
    }

    std::string json() const {
        boost::property_tree::ptree pt;
        message_protocol::json(pt);

        pt.add("client_sent", client_to_server.client_sent);
        pt.add("message", client_to_server.message);
        pt.add("server_received", server_received);

        std::stringstream ss;
        write_json(ss, pt);
        return ss.str();
    }
};

struct msg_question_files_current_directory : message_protocol {
    uint32_t max_length;

    msg_question_files_current_directory(boost::property_tree::ptree& pt)
        : message_protocol(question_files_current_directory, pt)
        , max_length(pt.get<uint32_t>("max_length")) {
    }
};

struct msg_response_files_current_directory : message_protocol {

    std::vector<std::string> files;

    msg_response_files_current_directory(const msg_question_files_current_directory&)
        :message_protocol(response_files_current_directory) {
    }

    std::string json() const {
        using namespace boost::property_tree;
        
        ptree pt;
        message_protocol::json(pt);

        // Many ways to create a json array with boost::property_tree:
        // http://stackoverflow.com/questions/tagged/json+boost-propertytree?sort=votes&pageSize=50
        ptree childs, element;
        for(auto it = files.begin(); it != files.end(); it++) {
            element.put_value(*it);
            childs.push_back(std::make_pair("", element));
        }
        pt.put_child(ptree::path_type("files"), childs);

        std::stringstream ss;
        write_json(ss, pt);
        return ss.str();
    }
};


