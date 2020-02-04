#include <iostream>
#include <unordered_map>
#include <numeric>
#include <chrono>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "./common.h"

#define MAXEVENTS 64

class server_config
{
public:
	enum PROTOCOL
	{
		TCP,
		UDP,
		UNKNOWN
	};

	server_config(std::string config_file)
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

		ipv4 = pt.get<std::string>("server.ip");
		port = std::stoi(pt.get<std::string>("server.port"));

		timeout = std::stoi(pt.get<std::string>("server.timeout"));

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

private:
	bool configured;
	std::string config_file;
	PROTOCOL protocol;
	std::string ipv4;
	int port;
	int timeout;
};
class tcp_server : communication
{
public:
	tcp_server(const std::string& ip, int port, size_t timeout)
	: is_running(false)
	, ipv4(ip)
	, port(port)
	, timeout(timeout)
	{
		memset(buf, '\0', sizeof (buf));
		memset(buf2, 'b', sizeof (buf2));
	}

	~tcp_server()
	{
		stop();
	}

	static int make_socket_non_blocking(int sfd)
	{
		int flags, s;

		flags = fcntl (sfd, F_GETFL, 0);
		if (flags == -1)
		{
			perror ("fcntl");
			return -1;
		}

		flags |= O_NONBLOCK;
		s = fcntl (sfd, F_SETFL, flags);
		if (s == -1)
		{
			perror ("fcntl");
			return -1;
		}

		return 0;
	}

	virtual int init_server()
	{
		sfd = create_and_bind();
		if (sfd == -1)
			abort ();

		s = tcp_server::make_socket_non_blocking(sfd);
		if (s == -1)
			abort ();
	}

	virtual int listen()
	{
		s = ::listen (sfd, SOMAXCONN);
		if (s == -1)
		{
			perror ("listen");
			abort ();
		}

		efd = epoll_create1(0);
		if (efd == -1)
		{
			perror ("epoll_create");
			abort ();
		}

		event.data.fd = sfd;
		event.events = EPOLLIN | EPOLLET;
		s = epoll_ctl (efd, EPOLL_CTL_ADD, sfd, &event);
		if (s == -1)
		{
			perror ("epoll_ctl");
			abort ();
		}

		/* Buffer where events are returned */
		events = (epoll_event*)calloc (MAXEVENTS, sizeof event);
	}

	virtual void start()
	{
		listen();
		is_running = true;
		/* The event loop */
		while (1)
		{
			int n, i;

			n = epoll_wait(efd, events, MAXEVENTS, 500);	// epoll timeout is based on miliseconds
			if (n == 0)	// Remove timeouted clients.
			{
				auto now = std::chrono::system_clock::now();
				for (auto element : client_timeout_list)
				{
					std::chrono::duration<double> elapsed_seconds = now - element.second;
					if (elapsed_seconds.count() > timeout)
					{
						std::cout << "time out for fd: " << element.first << std::endl;
						close(element.first);
						client_timeout_list.erase(element.first);
					}
				}
				continue;
			}
			for (i = 0; i < n; i++)
			{
				if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN)))
				{
					/* An error has occured on this fd, or the socket is not
					   ready for reading (why were we notified then?) */
					fprintf (stderr, "epoll error\n");
					close (events[i].data.fd);
					continue;
				}
				else if (sfd == events[i].data.fd)
				{
					/* We have a notification on the listening socket, which
					   means one or more incoming connections. */
					while (1)
					{
						struct sockaddr in_addr;
						socklen_t in_len;
						int infd;
						char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

						in_len = sizeof in_addr;
						infd = accept (sfd, &in_addr, &in_len);
						if (infd == -1)
						{
							if ((errno == EAGAIN) ||
									(errno == EWOULDBLOCK))
							{
								/* We have processed all incoming
								   connections. */
								break;
							}
							else
							{
								perror ("accept");
								break;
							}
						}

						s = getnameinfo (&in_addr, in_len,
								hbuf, sizeof hbuf,
								sbuf, sizeof sbuf,
								NI_NUMERICHOST | NI_NUMERICSERV);
						if (s == 0)
						{
							std::time_t now_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
							std::cout << std::ctime(&now_time) << std::endl;
							printf("Accepted connection on descriptor %d "
									"(host=%s, port=%s)\n", infd, hbuf, sbuf);
						}

						/* Make the incoming socket non-blocking and add it to the
						   list of fds to monitor. */
						s = tcp_server::make_socket_non_blocking (infd);
						if (s == -1)
							abort ();

						event.data.fd = infd;
						event.events = EPOLLIN | EPOLLET;
						s = epoll_ctl (efd, EPOLL_CTL_ADD, infd, &event);
						if (s == -1)
						{
							perror ("epoll_ctl");
							abort ();
						}

						client_timeout_list[infd] = std::chrono::system_clock::now();
					}
					continue;
				}
				else
				{
					/* We have data on the fd waiting to be read. Read and
					   display it. We must read whatever data is available
					   completely, as we are running in edge-triggered mode
					   and won't get a notification again for the same
					   data. */
					int done = 0;

					ssize_t count;

					while (1)
					{
						client_timeout_list[events[i].data.fd] = std::chrono::system_clock::now();
						count = read (events[i].data.fd, buf, sizeof buf);
						if (count == -1)
						{
							/* If errno == EAGAIN, that means we have read all
							   data. So go back to the main loop. */
							if (errno != EAGAIN)
							{
								perror ("read");
								done = 1;
							}
							break;
						}
						else if (count == 0)
						{
							/* End of file. The remote has closed the
							   connection. */
							done = 1;
							break;
						}

						/* Write the buffer to standard output */
						//std::cout << buf[0] << std::endl;
					}


					if (buf[0] == 'u')
						write (events[i].data.fd, buf, 1);
					else	// buf[0] == 'd'
						write (events[i].data.fd, buf2, sizeof buf2);

					if (done)
					{
						printf ("Closed connection on descriptor %d\n",
								events[i].data.fd);

						client_timeout_list.erase(events[i].data.fd);
						/* Closing the descriptor will make epoll remove it
						   from the set of descriptors which are monitored. */
						close (events[i].data.fd);
					}
				}
			}
		}
	}

	int send(char*, size_t) override
	{}
	int receive(char*, size_t) override
	{}

	virtual void stop()
	{
		if (is_running)
		{
			free (events);
			close (sfd);
			is_running = false;
		}
	}

private:

	int create_and_bind()
	{
		int sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd == -1)
			exit(1);

		struct sockaddr_in localaddr;
		localaddr.sin_family = AF_INET;
		localaddr.sin_addr.s_addr = inet_addr(ipv4.c_str());
		localaddr.sin_port = htons(port);
		int s = bind(sockfd, (struct sockaddr *)&localaddr, sizeof(localaddr));
		if (s != 0)
		{
			close(sockfd);
			exit(1);
		}
		return sockfd;
	}

	/* buffer for communication */
	char buf[BUF_SIZE];
	char buf2[BUF_SIZE];
	bool is_running;
	std::string ipv4;
	int port;
	size_t timeout;
	int sfd, s;
	int efd;
	std::unordered_map<int, std::chrono::time_point<std::chrono::system_clock>> client_timeout_list;
	struct epoll_event event;
	struct epoll_event *events;
};

int main (int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf (stderr, "Usage: %s [config file]\n", argv[0]);
		exit (EXIT_FAILURE);
	}

	server_config config(argv[1]);
	config.configure();
	if (config.get_protocol() == server_config::TCP)
	{
		tcp_server server(config.get_ip(), config.get_port(), config.get_timeout());
		server.init_server();
		server.start();
		server.stop();
	}
	else
	{
		std::cout << "The requested protocol is not implemented!" << std::endl;
	}
	return EXIT_SUCCESS;
}
