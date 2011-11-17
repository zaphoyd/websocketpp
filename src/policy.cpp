/*
build situation
general:
- libboost_system

SSL:
- libcrypto
- libssl

*/

/*#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>
#include <iostream>
#include <set>

class server_handler {
	virtual void on_action() = 0;
};

typedef boost::shared_ptr<server_handler> server_handler_ptr;

class client_handler {
	virtual void on_action() = 0;
};

typedef boost::shared_ptr<client_handler> client_handler_ptr;


namespace connection {
template <typename endpoint_type,template <class> class security_policy>
class connection 
	: public security_policy< connection<endpoint_type,security_policy> >,
	  public boost::enable_shared_from_this< connection<endpoint_type, security_policy> > {
public:
	typedef connection<endpoint_type,security_policy> type;
	typedef security_policy< type > security_policy_type;
	
	//typedef typename role_type::handler_ptr handler_ptr;
	
	connection(endpoint_type& e) : security_policy_type(e),m_endpoint(e) {
		std::cout << "setup connection" << std::endl;
	}
	
	void websocket_handshake() {
		std::cout << "Websocket Handshake" << std::endl;
		
		security_policy_type::get_socket().async_read_some(
			boost::asio::buffer(m_data, 512),
			boost::bind(
				&type::handle_read,
				type::shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred
			)
		);
		
		this->websocket_messages();
	}
	void websocket_messages() {
		std::cout << "Websocket Messages" << std::endl;
	}
	
	void handle_read(const boost::system::error_code& error,size_t bytes_transferred) {
		if (error) {
			std::cout << "read error" << std::endl;
		} else {
			std::cout << "read complete: " << m_data << std::endl;
			
			std::string r = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nbar";
			
			boost::asio::async_write(
				security_policy_type::get_socket(),
				boost::asio::buffer(r, r.size()),
				boost::bind(
					&type::handle_write,
					type::shared_from_this(),
					boost::asio::placeholders::error
				)
			);
		}
	}
	
	void handle_write(const boost::system::error_code& error) {
		if (error) {
			std::cout << "write error" << std::endl;
		} else {
			std::cout << "write successful" << std::endl;
			// end session
			m_endpoint.remove_connection(type::shared_from_this());
		}
	}
	
protected:
	endpoint_type& m_endpoint;
	char m_data[512];
};

// Connection roles
template <class connection_type>
class server {
public:
	typedef server_handler_ptr handler_ptr;
	
	server (handler_ptr h) : m_handler(h) {
		std::cout << "setup server connection role" << std::endl;
	}
	
private:
	handler_ptr	m_handler;
};

template <class connection_type>
class client {
public:
	typedef client_handler_ptr handler_ptr;
	
	client (handler_ptr h) : m_handler(h) {
		std::cout << "setup client connection role" << std::endl;
	}
	
private:
	handler_ptr m_handler;
};

}

namespace endpoint {

template <typename derived_t>
struct endpoint_traits;
	
class endpoint_base {
protected:
	boost::asio::io_service	m_io_service;
};
	
template <template <class> class role,template <class> class security_policy>
class endpoint 
 : public endpoint_base,
   public role< endpoint<role,security_policy> >,
   public security_policy< endpoint<role,security_policy> > 
{
public:
	typedef role< endpoint<role,security_policy> > role_type;
	typedef security_policy< endpoint<role,security_policy> > security_policy_type;
	typedef endpoint<role,security_policy> type;
		
	typedef typename role_type::handler_ptr handler_ptr;
	
	typedef connection::connection<type,security_policy< endpoint<role,security_policy> >::template connection> connection_type;
	typedef boost::shared_ptr<connection_type> connection_ptr;
	
	endpoint(handler_ptr h) : role_type(m_io_service,h),security_policy_type(m_io_service) {
		std::cout << "Setup endpoint" << std::endl;
	}
	
	void start() {
		std::cout << "Connect" << std::endl;
		
		connection_ptr con = create_connection();
		con->security_handshake();
	}
	
	connection_ptr create_connection() {
		connection_ptr new_connection(new connection_type(*this));
		m_connections.insert(new_connection);
		return new_connection;
	}
	
	void remove_connection(connection_ptr con) {
		m_connections.erase(con);
	}
private:
	std::set<connection_ptr> m_connections;
};

// endpoint related types that it's policy classes need.
template <template <class> class role,template <class> class security_policy>
struct endpoint_traits<endpoint<role, security_policy> > {
	typedef endpoint<role,security_policy> type;
	
	typedef connection::connection<type,security_policy< endpoint<role,security_policy> >::template connection> connection_type;
	typedef boost::shared_ptr<connection_type> connection_ptr;
};
	
// Security Policies
template <typename endpoint_type>
class plain {
public:
	boost::asio::io_service& get_io_service() {
		return m_io_service;
	}
	
	// Connection specific details
	template <typename connection_type>
	class connection {
	public:
		connection(plain<endpoint_type>& e) : m_socket(e.get_io_service()) {
			std::cout << "setup plain connection" << std::endl;
		}
		
		void security_handshake() {
			std::cout << "performing plain security handshake" << std::endl;
			static_cast< connection_type* >(this)->websocket_handshake();
		}
		
		boost::asio::ip::tcp::socket& get_raw_socket() {
			return m_socket;
		}
		
		boost::asio::ip::tcp::socket& get_socket() {
			return m_socket;
		}
	private:
		boost::asio::ip::tcp::socket m_socket;
	};
protected:
	plain (boost::asio::io_service& m) : m_io_service(m) {
		std::cout << "setup plain endpoint" << std::endl;
	}
private:
	boost::asio::io_service& m_io_service;
};

template <typename endpoint_type>
class ssl {
public:
	typedef ssl<endpoint_type> type;
	
	std::string get_password() const {
		return "test";
	}
	
	boost::asio::io_service& get_io_service() {
		return m_io_service;
	}
	
	boost::asio::ssl::context& get_context() {
		return m_context;
	}
	
	// Connection specific details
	template <typename connection_type>
	class connection {
	public:
		typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket;
		
		connection(ssl<endpoint_type>& e) : m_socket(e.get_io_service(),e.get_context()) {
			std::cout << "setup ssl connection" << std::endl;
		}
		
		void security_handshake() {
			std::cout << "performing ssl security handshake" << std::endl;
			
			m_socket.async_handshake(
				boost::asio::ssl::stream_base::server,
				boost::bind(
					&connection<connection_type>::handle_handshake,
					this,
					boost::asio::placeholders::error
				)
			);
		}
		
		void handle_handshake(const boost::system::error_code& error) {
			if (error) {
				std::cout << "SSL handshake error" << std::endl;
			} else {
				static_cast< connection_type* >(this)->websocket_handshake();
			}
		}
		
		ssl_socket::lowest_layer_type& get_raw_socket() {
			return m_socket.lowest_layer();
		}
		
		ssl_socket& get_socket() {
			return m_socket;
		}
	private:
		ssl_socket m_socket;
	};
protected:
	ssl (boost::asio::io_service& m) : m_io_service(m),m_context(boost::asio::ssl::context::sslv23) {
		std::cout << "setup ssl endpoint" << std::endl;
		
		try {
			m_context.set_options(boost::asio::ssl::context::default_workarounds |
								 boost::asio::ssl::context::no_sslv2 |
								 boost::asio::ssl::context::single_dh_use);
			m_context.set_password_callback(boost::bind(&type::get_password, this));
			m_context.use_certificate_chain_file("/Users/zaphoyd/Documents/websocketpp/src/ssl/server.pem");
			m_context.use_private_key_file("/Users/zaphoyd/Documents/websocketpp/src/ssl/server.pem", boost::asio::ssl::context::pem);
			m_context.use_tmp_dh_file("/Users/zaphoyd/Documents/websocketpp/src/ssl/dh512.pem");
		} catch (std::exception& e) {
			std::cout << e.what() << std::endl;
		}
	}
private:
	boost::asio::io_service& m_io_service;
	boost::asio::ssl::context m_context;
};

// Endpoint roles
template <class endpoint_type>
class server {
public:
	typedef server<endpoint_type> server_type;
	
	typedef typename endpoint_traits<endpoint_type>::connection_ptr connection_ptr;
	
	typedef server_handler_ptr handler_ptr; // duplicated typedef
	
	server(boost::asio::io_service& m,handler_ptr h) 
	 : m_handler(h),
	   m_io_service(m),
	   m_endpoint(),
	   m_acceptor(m) {
		std::cout << "setup server endpoint role" << std::endl;
	}
	
	void listen(unsigned short port) {
		m_endpoint.port(port);
		m_acceptor.open(m_endpoint.protocol());
		m_acceptor.set_option(boost::asio::socket_base::reuse_address(true));
		m_acceptor.bind(m_endpoint);
		m_acceptor.listen();
		
		this->accept();		
		
		
		m_io_service.run();
	}

	void connect() {
		static_cast< endpoint_type* >(this)->start();
	}
	
	void public_api() {
		std::cout << "endpoint::server::public_api()" << std::endl;
	}
protected:
	handler_ptr get_handler() {
		return m_handler;
	}
	
	void protected_api() {
		std::cout << "endpoint::server::protected_api()" << std::endl;
	}
private:
	void accept() {
		connection_ptr con = static_cast< endpoint_type* >(this)->create_connection();
			
		m_acceptor.async_accept(
			con->get_raw_socket(),

			boost::bind(
				&server_type::handle_accept,
				this,
				con,
				boost::asio::placeholders::error
			)
		);
	}
	
	void handle_accept(connection_ptr con, const boost::system::error_code& error) {
		if (!error) {
			con->security_handshake();
		} else {
			throw "";
		}
		
		this->accept();
	}

	void private_api() {
		std::cout << "endpoint::server::private_api()" << std::endl;
	}
	handler_ptr						m_handler;
	boost::asio::io_service&		m_io_service;
	boost::asio::ip::tcp::endpoint	m_endpoint;
	boost::asio::ip::tcp::acceptor	m_acceptor;
};

template <class endpoint_type>
class client {
public:
	//typedef connection::client connection_type;
	typedef client_handler_ptr handler_ptr; // duplicated typedef
	
	client (boost::asio::io_service& m,handler_ptr h) : m_handler(h),m_io_service(m) {
		std::cout << "setup client endpoint role" << std::endl;
	}
	
	void connect() {
		static_cast< endpoint_type* >(this)->start();
	}
	
	void public_api() {
		std::cout << "endpoint::client::public_api()" << std::endl;
	}
protected:
	handler_ptr get_handler() {
		return m_handler;
	}
	
	void protected_api() {
		std::cout << "endpoint::client::protected_api()" << std::endl;
	}
private:
	void private_api() {
		std::cout << "endpoint::client::private_api()" << std::endl;
	}
	handler_ptr m_handler;
	boost::asio::io_service& m_io_service;
};

template <template <class> class endpoint_role,template <class> class security_type>
class factory {
public:
	typedef endpoint<endpoint_role,security_type> endpoint_type;
	typedef endpoint_role<endpoint_type> endpoint_interface;
	typedef boost::shared_ptr<endpoint_interface> endpoint_ptr;
	
	typedef typename endpoint_type::handler_ptr handler_ptr;
	
	endpoint_ptr create(handler_ptr h) {
		return endpoint_ptr(new endpoint_type(h));
	}
};

}*/

// Application start
#include "endpoint.hpp"
#include "roles/server.hpp"
#include "sockets/ssl.hpp"

typedef websocketpp::endpoint<websocketpp::role::server,websocketpp::socket::plain> endpoint_type;
typedef websocketpp::role::server<endpoint_type>::handler handler_type;
typedef websocketpp::role::server<endpoint_type>::handler_ptr handler_ptr;

// application headers
class application_server_handler : public handler_type {
	void on_action() {
		std::cout << "application_server_handler::on_action()" << std::endl;
	}
};

/*class application_client_handler : public client_handler {
	void on_action() {
		std::cout << "application_client_handler::on_action()" << std::endl;
	}
};*/


int main () {
	std::cout << "Endpoint 0" << std::endl;
	handler_ptr h(new application_server_handler());
	endpoint_type e(h);

	e.listen(9000);
	//e.connect();
	//e.public_api();
	std::cout << std::endl;

	return 0;
}