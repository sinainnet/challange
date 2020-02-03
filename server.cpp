#include <iostream>
#include <unordered_map>

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

#include "./common.h"

#define MAXEVENTS 64

class tcp_server : communication
{
public:
	tcp_server(const std::string& port)
	: is_running(false)
	, port(port)
	{
		memset(buf, '\0', sizeof (buf));
		memset(buf2, 'b', sizeof (buf2));
	}

	~tcp_server()
	{
		stop();
	}

	virtual int init_server()
	{
		sfd = create_and_bind();
		if (sfd == -1)
			abort ();

		s = make_socket_non_blocking(sfd);
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

		efd = epoll_create1 (0);
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

			n = epoll_wait (efd, events, MAXEVENTS, -1);
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
							printf("Accepted connection on descriptor %d "
									"(host=%s, port=%s)\n", infd, hbuf, sbuf);
						}

						/* Make the incoming socket non-blocking and add it to the
						   list of fds to monitor. */
						s = make_socket_non_blocking (infd);
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
						std::cout << buf[0] << std::endl;
					}


					if (buf[0] == 'u')
						write (events[i].data.fd, buf, 1);
					else	// buf[0] == 'd'
						write (events[i].data.fd, buf2, sizeof buf2);

					if (done)
					{
						printf ("Closed connection on descriptor %d\n",
								events[i].data.fd);

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

	int create_and_bind ()
	{
		struct addrinfo hints;
		struct addrinfo *result, *rp;
		int s, sfd;

		memset (&hints, 0, sizeof (struct addrinfo));
		hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
		hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */
		hints.ai_flags = AI_PASSIVE;     /* All interfaces */

		s = getaddrinfo (NULL, port.c_str(), &hints, &result);
		if (s != 0)
		{
			fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (s));
			return -1;
		}

		for (rp = result; rp != NULL; rp = rp->ai_next)
		{
			sfd = socket (rp->ai_family, rp->ai_socktype, rp->ai_protocol);
			if (sfd == -1)
				continue;

			s = bind (sfd, rp->ai_addr, rp->ai_addrlen);
			if (s == 0)
			{
				/* We managed to bind successfully! */
				break;
			}

			close (sfd);
		}

		if (rp == NULL)
		{
			fprintf (stderr, "Could not bind\n");
			return -1;
		}

		freeaddrinfo (result);

		return sfd;
	}

	int make_socket_non_blocking(int sfd)
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

	/* buffer for communication */
	char buf[BUF_SIZE];
	char buf2[BUF_SIZE];
	bool is_running;
	std::string port;
	int sfd, s;
	int efd;
	std::unordered_map<int, size_t> client_timeout_list;
	struct epoll_event event;
	struct epoll_event *events;
};

int main (int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf (stderr, "Usage: %s [port]\n", argv[0]);
		exit (EXIT_FAILURE);
	}

	tcp_server server(argv[1]);
	server.init_server();
	server.start();
	server.stop();
	return EXIT_SUCCESS;
}
