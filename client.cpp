#include <iostream>
#include <string>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <fcntl.h> // for open
#include <unistd.h> // for close
#include <pthread.h>
#include "./common.h"

class client_config
{
public:
	enum PROTOCOL
	{
		TCP,
		UDP,
		UNKNOWN
	};

	client_config(std::string config_file)
	: configured(false)
	, config_file(config_file)
	{}
	void configure()
	{
		configured = false;

		boost::property_tree::ptree pt;
		boost::property_tree::ini_parser::read_ini(config_file.c_str(), pt);

		if (pt.get<std::string>("server.protocol") == "tcp")
			protocol = TCP;
		else if (pt.get<std::string>("server.protocol") == "udp")
			protocol = UDP;
		else
			protocol = UNKNOWN;

		std::string full_address = pt.get<std::string>("server.address");
		std::string delimiter = ":";
		size_t pos = 0;
		pos = full_address.find(delimiter);
		ipv4 = full_address.substr(0, pos);
		full_address.erase(0, pos + delimiter.length());
		port = std::stoi(full_address);

		timeout = std::stoi(pt.get<std::string>("server.timeout"));

		download_time = std::stoi(pt.get<std::string>("test.download_time"));
		upload_time = std::stoi(pt.get<std::string>("test.upload_time"));

		configured = true;
	}

	PROTOCOL get_protocol()
	{
		if (configured)
			return protocol;
		else
			return UNKNOWN;
	}

	std::string get_ip()
	{
		if (configured)
			return ipv4;
		else
			return nullptr;
	}

	int get_port()
	{
		if (configured)
			return port;
		else
			return -1;
	}

	int get_timeout()
	{
		if (configured)
			return timeout;
		else
			return -1;
	}

	int get_download_time()
	{
		if (configured)
			return download_time;
		else
			return -1;
	}

	int get_upload_time()
	{
		if (configured)
			return upload_time;
		else
			return -1;
	}

private:
	bool configured;
	std::string config_file;
	PROTOCOL protocol;
	std::string ipv4;
	int port;
	int timeout;
	int download_time;
	int upload_time;
};

class tcp_client : communication
{
public:
	tcp_client(std::string ipv4, int port)
	: clientPort(port)
	, serverIpv4(ipv4)
	, is_connected(false)
	{
	}

	~tcp_client()
	{
		disconnect();
	}

	virtual int connect(size_t timeout = 0)
	{
		if (is_connected)
			return -1;

		// TCP related socket: SOCK_STREAM
		clientSocket = socket(PF_INET, SOCK_STREAM, 0);
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_port = htons(clientPort);
		serverAddr.sin_addr.s_addr = inet_addr(serverIpv4.c_str());
		memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);
		addr_size = sizeof serverAddr;

		if (timeout == 0)
		{
			if (::connect(clientSocket, (struct sockaddr *) &serverAddr, addr_size) < 0)
			{
				std::cout << "Connect failed" << std::endl;
				return -1;
			}

			is_connected = true;
			return 0;
		}
		else
		{
			int res;
			long arg;
			fd_set myset;
			struct timeval tv;
			int valopt;
			socklen_t lon;

			// Set non-blocking
			if( (arg = fcntl(clientSocket, F_GETFL, NULL)) < 0)
			{
				fprintf(stderr, "Error fcntl(..., F_GETFL) (%s)\n", strerror(errno));
				exit(0);
			}
			arg |= O_NONBLOCK;
			if( fcntl(clientSocket, F_SETFL, arg) < 0)
			{
				fprintf(stderr, "Error fcntl(..., F_SETFL) (%s)\n", strerror(errno));
				exit(0);
			}

			// Trying to connect with timeout
			res = ::connect(clientSocket, (struct sockaddr *)&serverAddr, addr_size);
			if (res < 0)
			{
				if (errno == EINPROGRESS)
				{
					//fprintf(stderr, "EINPROGRESS in connect() - selecting\n");
					do
					{
						tv.tv_sec = timeout;
						tv.tv_usec = 0;
						FD_ZERO(&myset);
						FD_SET(clientSocket, &myset);
						res = select(clientSocket+1, NULL, &myset, NULL, &tv);
						if (res < 0 && errno != EINTR)
						{
							fprintf(stderr, "Error connecting %d - %s\n", errno, strerror(errno));
							exit(0);
						}
						else if (res > 0)
						{
							// Socket selected for write
							lon = sizeof(int);
							if (getsockopt(clientSocket, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &lon) < 0)
							{
								fprintf(stderr, "Error in getsockopt() %d - %s\n", errno, strerror(errno));
								exit(0);
							}
							// Check the value returned...
							if (valopt)
							{
								fprintf(stderr, "Error in delayed connection() %d - %s\n", valopt, strerror(valopt));
								exit(0);
							}

							break;
						}
						else
						{
							fprintf(stderr, "Timeout in select() - Cancelling!\n");
							exit(0);
						}
					} while (1);
				}
				else
				{
					fprintf(stderr, "Error connecting %d - %s\n", errno, strerror(errno));
					exit(0);
				}
			}
			// Set to blocking mode again...
			if( (arg = fcntl(clientSocket, F_GETFL, NULL)) < 0) 
			{
				fprintf(stderr, "Error fcntl(..., F_GETFL) (%s)\n", strerror(errno));
				exit(0);
			}
			arg &= (~O_NONBLOCK);
			if( fcntl(clientSocket, F_SETFL, arg) < 0)
			{
				fprintf(stderr, "Error fcntl(..., F_SETFL) (%s)\n", strerror(errno));
				exit(0);
			}
			is_connected = true;
			return 0;
		}
	}

	int send(char* buf, size_t n) override
	{
		if (!is_connected)
			return -1;

		int sent = ::send(clientSocket , buf , n , 0);
		if (sent < 0)
		{
			std::cout << "Send failed" << std::endl;
		}
		return sent;
	}

	int receive(char* buf, size_t n) override
	{
		if (!is_connected)
			return -1;

		int received = ::recv(clientSocket, buf, n, 0);
		if (received < 0)
		{
			std::cout << "Receive failed" << std::endl;
		}
		return received;
	}

	virtual int disconnect()
	{
		if (!is_connected)
			return -1;

		int res = close(clientSocket);
		if (res < 0)
		{
			std::cout << "Close socket failed" << std::endl;
		}
		is_connected = false;
	}

private:
	int clientPort;
	std::string serverIpv4;
	bool is_connected;
	int clientSocket;
	struct sockaddr_in serverAddr;
	socklen_t addr_size;
};

bool run = true;

void clock_tick(const boost::system::error_code& /*e*/)
{
	std::cout << "time tick!" << std::endl;
	run = false;
}

void* timer_thread(void *ptr)
{
	boost::asio::io_service clock_io;

	size_t wait_time = *(size_t*)(ptr);
	boost::asio::deadline_timer t(clock_io, boost::posix_time::seconds(wait_time));
	t.async_wait(&clock_tick);

	clock_io.run();
}

void client(std::string config_file)
{
	client_config config(config_file);
	config.configure();
	//std::cout << config.get_protocol() << "\n" << config.get_ip() << "\n" << config.get_port() << "\n" << config.get_timeout() << std::endl;
	//std::cout << config.get_download_time() << "\n" << config.get_upload_time() << std::endl;

	if (config.get_protocol() == client_config::TCP)
	{
		char message[BUF_SIZE];
		char buffer[BUF_SIZE];
		tcp_client client(config.get_ip(), config.get_port());
		client.connect(config.get_timeout());

		std::cout << "Try to upload" << std::endl;
		memset(message, 'a', sizeof (message));
		message[0] = 'u';

		/* this variable is our reference to the second thread */
		pthread_t thread;

		size_t upload_time = config.get_upload_time();
		/* create a second thread which executes inc_x(&x) */
		if(pthread_create(&thread, NULL, timer_thread, &upload_time)) {

			fprintf(stderr, "Error creating thread\n");
			return;
		}

		size_t total_upload = 0;
		while (run)
		{
			total_upload += client.send(message, BUF_SIZE);
			client.receive(buffer, 1);
		}

		if(pthread_join(thread, NULL)) {

			fprintf(stderr, "Error joining thread\n");
			return;
		}

		client.disconnect();

		std::cout << "Upload " << ((double)total_upload)/(upload_time * (1 << 20)) << " MBps" << "(for " << upload_time << " seconds)" << std::endl;


		client.connect(config.get_timeout());
		std::cout << "Try to download" << std::endl;
		message[0] = 'd';

		run = true;
		size_t download_time = config.get_download_time();
		if(pthread_create(&thread, NULL, timer_thread, &download_time)) {

			fprintf(stderr, "Error creating thread\n");
			return;
		}

		size_t total_download = 0;
		while (run)
		{
			client.send(message, 1);
			total_download += client.receive(buffer, BUF_SIZE);
		}

		if(pthread_join(thread, NULL)) {

			fprintf(stderr, "Error joining thread\n");
			return;
		}

		client.disconnect();
		std::cout << "Download " << ((double)total_download)/(download_time * (1 << 20)) << " MBps" << "(for " << download_time << " seconds)" << std::endl;
	}
	else
	{
		std::cout << "PROTOCOL " << config.get_protocol() << " is not implemented!" << std::endl;
	}
}
int main(int argc, const char* argv[]){
	if (argc != 2)
		exit(1);

	client(argv[1]);
	return 0;
}
