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
#include<pthread.h>
#include "./common.h"

class tcp_client : server
{};

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

void client()
{
	printf("In thread\n");
	char message[32000];
	char buffer[34000];
	int clientSocket;
	struct sockaddr_in serverAddr;
	socklen_t addr_size;
	// Create the socket.
	clientSocket = socket(PF_INET, SOCK_STREAM, 0);
	//Configure settings of the server address
	// Address family is Internet
	serverAddr.sin_family = AF_INET;
	//Set port number, using htons function
	serverAddr.sin_port = htons(8000);
	//Set IP address to localhost
	serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);
	//Connect the socket to the server using the address
	addr_size = sizeof serverAddr;
	connect(clientSocket, (struct sockaddr *) &serverAddr, addr_size);
	//strcpy(message,"Hello");
	memset(message, 'a', sizeof (message));
	buffer[0] = 'c';

	/* this variable is our reference to the second thread */
	pthread_t thread;

	/* create a second thread which executes inc_x(&x) */
	if(pthread_create(&thread, NULL, timer_thread, NULL)) {

		fprintf(stderr, "Error creating thread\n");
		return;
	}


	while (buffer[0] == 'c' && run)
	{
		if( send(clientSocket , message , strlen(message) , 0) < 0)
		{
			printf("Send failed\n");
		}
		//Read the message from the server into the buffer
		if(recv(clientSocket, buffer, 34000, 0) < 0)
		{
			printf("Receive failed\n");
		}
	}

	{
		message[0] = 'e';
		if( send(clientSocket , message , strlen(message) , 0) < 0)
		{
			printf("Send failed\n");
		}
		//Read the message from the server into the buffer
		if(recv(clientSocket, buffer, 34000, 0) < 0)
		{
			printf("Receive failed\n");
		}
	}
	//Print the received message
	//printf("Data received: %s\n",buffer);
	if (buffer[0] == 'd')
		std::cout << buffer << std::endl;
	close(clientSocket);
}
int main(){

	client();
	return 0;
}
