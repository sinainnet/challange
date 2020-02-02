#include <ctime>
#include <iostream>
#include <string>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>

#include "./common.h"

using boost::asio::ip::tcp;

std::string send_buf(BUF_SIZE, 'a');

class tcp_connection
: public boost::enable_shared_from_this<tcp_connection>
{
	public:
		typedef boost::shared_ptr<tcp_connection> pointer;

		static pointer create(boost::asio::io_service& io_service)
		{
			return pointer(new tcp_connection(io_service));
		}

		tcp::socket& socket()
		{
			return socket_;
		}

		void start()
		{
			//message_ = make_daytime_string();
			while (client_request == DOWNLOAD)
			{
				handle_request();

				std::cout << client_request << std::endl;

				//boost::asio::async_write(socket_, boost::asio::buffer(message_),
				boost::asio::async_write(socket_, boost::asio::buffer(send_buf),
						boost::bind(&tcp_connection::handle_write, shared_from_this(),
							boost::asio::placeholders::error,
							boost::asio::placeholders::bytes_transferred));
			}
		}

	private:
		enum COMMAND
		{
			DOWNLOAD,
			UPLOAD
		};

		tcp_connection(boost::asio::io_service& io_service)
			: socket_(io_service)
		{
		}

		//void handle_request(const boost::system::error_code& err)
		void handle_request()
		{
			boost::array<char, 2> buf;
			boost::system::error_code error;

			size_t len = socket_.read_some(boost::asio::buffer(buf), error);
			if (buf[0] == 'd')
				client_request = DOWNLOAD;
			else
				client_request = UPLOAD;
		}

		void handle_write(const boost::system::error_code& /*error*/,
				size_t /*bytes_transferred*/)
		{
		}

		//void handle_read(const boost::system::error_code& /*error*/,
		//		size_t /*bytes_transferred*/)
		//{
		//}

		tcp::socket socket_;
		std::string message_;
		COMMAND client_request;
};

class tcp_server : server
{
	public:
		tcp_server(boost::asio::io_service& io_service)
			: acceptor_(io_service, tcp::endpoint(tcp::v4(), 13))
		{
			start_accept();
		}

	private:
		void start_accept()
		{
			tcp_connection::pointer new_connection =
				tcp_connection::create(acceptor_.get_io_service());

			acceptor_.async_accept(new_connection->socket(),
					boost::bind(&tcp_server::handle_accept, this, new_connection,
						boost::asio::placeholders::error));
		}

		void handle_accept(tcp_connection::pointer new_connection,
				const boost::system::error_code& error)
		{
			if (!error)
			{
				new_connection->start();
			}

			start_accept();
		}

		tcp::acceptor acceptor_;
};

int main()
{
	try
	{
		boost::asio::io_service io_service;
		tcp_server server(io_service);
		io_service.run();
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

	return 0;
}
