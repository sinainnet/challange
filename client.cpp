#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

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

	int connect() override
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
		std::cout << "Connecting" << std::endl;
		if (::connect(clientSocket, (struct sockaddr *) &serverAddr, addr_size) < 0)
		{
			std::cout << "Connect failed" << std::endl;
			return -1;
		}

		std::cout << "Connected" << std::endl;
		is_connected = true;
		return 0;
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

	int disconnect() override
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

	boost::asio::deadline_timer t(clock_io, boost::posix_time::seconds(DEFUALT_WAIT));
	t.async_wait(&clock_tick);

	clock_io.run();
}

void client()
{
	printf("In thread\n");
	char message[32000];
	char buffer[32000];
	tcp_client client("127.0.0.1", 8000);
	client.connect();
	memset(message, 'a', sizeof (message));
	message[0] = 'u';

	/* this variable is our reference to the second thread */
	pthread_t thread;

	/* create a second thread which executes inc_x(&x) */
	if(pthread_create(&thread, NULL, timer_thread, NULL)) {

		fprintf(stderr, "Error creating thread\n");
		return;
	}

	while (run)
	{
		client.send(message, 32000);
		client.receive(buffer, 1);
	}

	if(pthread_join(thread, NULL)) {

		fprintf(stderr, "Error joining thread\n");
		return;
	}

	client.disconnect();

	client.connect();
	message[0] = 'd';

	run = true;
	if(pthread_create(&thread, NULL, timer_thread, NULL)) {

		fprintf(stderr, "Error creating thread\n");
		return;
	}

	while (run)
	{
		client.send(message, 1);
		client.receive(buffer, 32000);
	}

	if(pthread_join(thread, NULL)) {

		fprintf(stderr, "Error joining thread\n");
		return;
	}

	client.disconnect();
}
int main(){

	client();
	return 0;
}
