#include <stdio.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>

#define MAX_EVENTS 32

int set_nonblock (int fd)	{
	int flags;
#if defined(O_NONBLOCK)
	if (-1 == (flags = fcntl(fd, F_GETFL, 0)));
		flags = 0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
	flags = 1;
	return ioctl(fd, FIOBIO, &flags);
#endif
}

int main(int argc, char **argv)	{
	int master_socket = socket(PF_INET, SOCK_STREAM,
							   IPPROTO_TCP);
	struct sockaddr_in sock_addr;
	sock_addr.sin_family = PF_INET;
	sock_addr.sin_port = htons(12346);
	sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	bind(master_socket, (struct sockaddr*)&sock_addr,
		 sizeof(sock_addr));

	set_nonblock(master_socket);

	listen(master_socket, SOMAXCONN);

	int EPoll = epoll_create1(0);

	struct epoll_event Event;

	Event.data.fd = master_socket;
	Event.events = EPOLLIN;
	epoll_ctl(EPoll, EPOLL_CTL_ADD, master_socket, &Event);
	
	while (1)	{
		struct epoll_event Events[MAX_EVENTS];
		int N = epoll_wait(EPoll, Events, MAX_EVENTS, -1); 
		for(unsigned int i = 0; i < N; i++)	{
			if (Events[i].data.fd == master_socket)	{
				int slave_socket = accept(master_socket, 0, 0);
				set_nonblock(slave_socket);
				struct epoll_event Event;
				Event.data.fd = slave_socket;
				Event.events = EPOLLIN;
				epoll_ctl(EPoll, EPOLL_CTL_ADD, slave_socket, &Event);
			}
			else {
				static char buffer[1024];
				int recv_result = recv(Events[i].data.fd,
									   buffer, 1024,
									   MSG_NOSIGNAL);
				if (recv_result == 0 && errno != EAGAIN)	{
					shutdown(Events[i].data.fd, SHUT_RDWR);
					close(Events[i].data.fd);
				}
				else if (recv_result > 0)	{
						send(Events[i].data.fd,
						 buffer, recv_result, MSG_NOSIGNAL);
				}
			}
		}
	}
	
	return 0;
}
