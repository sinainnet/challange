#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <sys/time.h>
#include <time.h>
#include <pthread.h>

#include "./common.h"

using boost::asio::ip::tcp;

class tcp_client : server
{};

// Modify
//long time_left(struct timespec& start, struct timespec& finish)
//{
//	long seconds = finish->tv_sec - start->tv_sec;
//	long ns = finish->tv_nsec - start->tv_nsec;
//
//	if (start.tv_nsec > finish.tv_nsec) { // clock underflow
//		--seconds;
//		ns += 1000000000;
//	}
//
//	ns += seconds * 1000000000;
//
//	return ns;
//}

bool run = true;

void clock_tick(const boost::system::error_code& /*e*/)
{
	std::cout << "Hello, world!" << std::endl;
	run = false;
}

void* timer_thread(void *ptr)
{
	boost::asio::io_service clock_io;

	boost::asio::deadline_timer t(clock_io, boost::posix_time::seconds(DEFUALT_WAIT));
	t.async_wait(&clock_tick);

	clock_io.run();
}

int main(int argc, char* argv[])
{
	struct timespec start, finish;

	try
	{
		if (argc != 2)
		{
			std::cerr << "Usage: client <host>" << std::endl;
			return 1;
		}

		boost::asio::io_service io_service;

		tcp::resolver resolver(io_service);
		tcp::resolver::query query(argv[1], "daytime");
		tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

		tcp::socket socket(io_service);
		boost::asio::connect(socket, endpoint_iterator);

		boost::array<char, 2> req;
		req[0] = 'd';
		boost::system::error_code req_error;
		boost::array<char, BUF_SIZE> buf;
		boost::system::error_code error;

		size_t total_download = 0;
		size_t total_upload = 0;

		pthread_t thread1;
		pthread_create(&thread1, NULL, timer_thread, NULL);
		while (run)
		{
			socket.write_some(boost::asio::buffer(req), req_error);

			if (req_error == boost::asio::error::eof)
				return 0; // Connection closed cleanly by peer.
			else if (req_error)
				throw boost::system::system_error(req_error); // Some other error.

			size_t len = socket.read_some(boost::asio::buffer(buf), error);

			if (error == boost::asio::error::eof)
				break; // Connection closed cleanly by peer.
			else if (error)
				throw boost::system::system_error(error); // Some other error.

			total_download += len;

			socket.write_some(boost::asio::buffer(req), req_error);
		}
		pthread_join( thread1, NULL);
		std::cout << ((double)(total_download))/(DEFUALT_WAIT * (1 << 20)) << "MBps" << std::endl;

		// Write some thing
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

	return 0;
}
