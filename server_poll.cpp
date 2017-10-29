#include <iostream>

#include <set>
#include <algorithm>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#define POLL_SIZE 2048

int set_nonblock (int fd)	{
	int flags;
#if defined (O_NONBLOCK)
	if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
		flags = 0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
	flags = 1;
	return ioctl(fd, FIOBIO, &flags);
#endif
}

int main(int argc, char **argv)	{
	int master_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	std::set<int> slave_sockets;
	
	struct sockaddr_in sock_addr;
	sock_addr.sin_family = PF_INET;
	sock_addr.sin_port = htons(12345);
	sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(master_socket, (struct sockaddr*)&sock_addr,
		 sizeof(sock_addr));

	set_nonblock(master_socket);

	listen(master_socket, SOMAXCONN);

	struct pollfd Set[POLL_SIZE];
	Set[0].fd = master_socket;
	Set[0].events = POLLIN;
	
	while(1)	{

		unsigned int index = 1;
		for (auto iter = slave_sockets.begin(); iter != slave_sockets.end();
			 iter++)	{
			Set[index].fd = *iter;
			Set[index].events = POLLIN;
			index++;
		}

		unsigned int set_size = 1 + slave_sockets.size();

		poll(Set, set_size, -1);

		for (unsigned int i = 0; i < set_size; i++)	{
			if (Set[i].revents & POLLIN) {
				if (i) {
					static char buffer[1024];
					int recv_size = recv(Set[i].fd,
										 buffer, 1024, MSG_NOSIGNAL);
					if (recv_size == 0 && errno != EAGAIN)	{
						shutdown(Set[i].fd, SHUT_RDWR);
						close(Set[i].fd);
						slave_sockets.erase(Set[i].fd);
					}
					else if (recv_size > 0)
						send(Set[i].fd, buffer, recv_size, MSG_NOSIGNAL);
				}
				else {
					int slave_socket = accept(master_socket, 0, 0);
					set_nonblock(slave_socket);
					slave_sockets.insert(slave_socket);
				}
			}
		}
	}
	
	return 0;
}
