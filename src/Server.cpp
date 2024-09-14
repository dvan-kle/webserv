#include "../include/Server.hpp"
#include "../include/Request.hpp"

Server::Server(int port)
{
	// Create socket
	_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (_sock == -1)
	{
		std::cerr << "Error: socket creation failed" << std::endl;
		exit(EXIT_FAILURE);
	}
	// Set socket options
	int opt = 1;
	if (setsockopt(_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
	{
		std::cerr << "Error: setsockopt failed" << std::endl;
		close(_sock);
		exit(EXIT_FAILURE);
	}
	// Bind the socket to the port
	_address.sin_family = AF_INET;
	_address.sin_addr.s_addr = INADDR_ANY;
	_address.sin_port = htons(port);
	if (bind(_sock, (struct sockaddr*)&_address, sizeof(_address)) == -1)
	{
		std::cerr << "Error: bind failed" << std::endl;
		close(_sock);
		exit(EXIT_FAILURE);
	}
	// Listen for incoming connections
	listen(_sock, 4096);
	
	EpollCreate();
	
	std::cout << "Server is listening with address " << inet_ntoa(_address.sin_addr) << " on port " << port << std::endl;
	
	EpollWait();
}
void Server::EpollCreate()
{
	// Create epoll instance
	_epoll_fd = epoll_create1(0);
	if (_epoll_fd == -1)
	{
		std::cerr << "Error: epoll_create1 failed" << std::endl;
		close(_sock);
		exit(EXIT_FAILURE);
	}
	// Add the listening socket to epoll
	_event.events = EPOLLIN;
	_event.data.fd = _sock;
	if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _sock, &_event) == -1)
	{
		std::cerr << "Error: epoll_ctl failed" << std::endl;
		close(_sock);
		exit(EXIT_FAILURE);
	}
}

void Server::EpollWait()
{
	while (true)
	{
		int i = 0;
		std::cout << "Times ran: " << i++ << std::endl;
		int nfds = epoll_wait(_epoll_fd, _events, MAX_EVENTS, -1);
		if (nfds == -1)
		{
			std::cerr << "Error: epoll_wait failed" << std::endl;
			close(_sock);
			exit(EXIT_FAILURE);
		}
		for (int n = 0; n < nfds; ++n)
		{
			if (_events[n].data.fd == _sock)
			{
				_conn_sock = accept(_sock, NULL, NULL);
				if (_conn_sock == -1)
				{
					std::cerr << "Error: accept failed" << std::endl;
					close(_sock);
					exit(EXIT_FAILURE);
				}
				_event.events = EPOLLIN | EPOLLET;
				_event.data.fd = _conn_sock;
				if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _conn_sock, &_event) == -1)
				{
					std::cerr << "Error: epoll_ctl failed" << std::endl;
					close(_sock);
					exit(EXIT_FAILURE);
				}
			}
			else
			{	
				HandleClient(_events[n].data.fd);
			}
		}
	}
}

void Server::HandleClient(int client_fd)
{
	Request request(client_fd);
	request.ParseRequest();
	
}

Server::~Server()
{
	close(_sock);
}
