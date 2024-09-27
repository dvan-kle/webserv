#include "../include/Server.hpp"
#include "../include/Request.hpp"

Server::Server(const std::vector<int> &ports, const std::vector<std::string> &hosts, const std::vector<ServerConfig> &servers)
{
	for (size_t i = 0; i < ports.size(); ++i) {
		CreateSocket(ports[i], hosts[i]);
	}
	EpollCreate();
	std::cout << YELLOW << "Server is listening on address:" << BLUE << std::endl;
	for (std::string host : hosts) {
		std::cout << host << std::endl;
	}
	std::cout << YELLOW << "with ports:" << GREEN << std::endl;
	for (int port : ports) {
		std::cout <<  port << std::endl;
	}
	std::cout << RESET << std::endl;
	EpollWait(servers);
}

void Server::CreateSocket(int port, const std::string &ip)
{
	// Create socket 
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		std::cerr << "Error: socket creation failed" << std::endl;
		exit(EXIT_FAILURE);
	}
	// Set socket options
	int opt = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
		std::cerr << "Error: setsockopt failed" << std::endl;
		close(sock);
		exit(EXIT_FAILURE);
	}
	// Bind the socket to the port
	_address.sin_family = AF_INET;
	_address.sin_port = htons(port);

	// Convert ip string to binary form
	if (ip == "localhost")
		_address.sin_addr.s_addr = INADDR_ANY;
	else {
	 if (inet_pton(AF_INET, ip.c_str(), &_address.sin_addr) <= 0) {
        std::cerr << "Error: Invalid IP address" << std::endl;
        close(sock);
        exit(EXIT_FAILURE);
    }
	}
	if (bind(sock, (struct sockaddr*)&_address, sizeof(_address)) == -1) {
		std::cerr << RED << "Error: bind failed" << RESET << std::endl;
		std::cout << BLUE << "	Check your IP address and port" << RESET << std::endl;
		close(sock);
		exit(EXIT_FAILURE);
	}
	// Listen for incoming connections
	listen(sock, 4096);

	_socks.push_back(sock);
}

void Server::EpollCreate()
{
	// Create epoll instance
	_epoll_fd = epoll_create1(0);
	if (_epoll_fd == -1) {
		std::cerr << "Error: epoll_create1 failed" << std::endl;
		exit(EXIT_FAILURE);
	}

	// Add the listening socket to epoll
	for (int sock : _socks) {
		_event.events = EPOLLIN;
		_event.data.fd = sock;
		if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, sock, &_event) == -1) {
			std::cerr << "Error: epoll_ctl failed" << std::endl;
			close(sock);
			exit(EXIT_FAILURE);
		}
	}
}

void Server::EpollWait(const std::vector<ServerConfig> &servers)
{
	while (true) {
		int nfds = epoll_wait(_epoll_fd, _events, MAX_EVENTS, -1);
		if (nfds == -1) {
			std::cerr << "Error: epoll_wait failed" << std::endl;
			exit(EXIT_FAILURE);
		}
		for (int n = 0; n < nfds; ++n) {
			int fd = _events[n].data.fd;

			if (std::find(_socks.begin(), _socks.end(), fd) != _socks.end()) {
				_conn_sock = accept(fd, NULL, NULL);
				if (_conn_sock == -1) {
					std::cerr << "Error: accept failed" << std::endl;
					continue;
				}

				//Store the port the client is connected to
				int server_port;
				socklen_t len = sizeof(_address);
				getsockname(_conn_sock, (struct sockaddr *)&_address, &len);
				server_port = ntohs(_address.sin_port);

				_client_port[_conn_sock] = server_port;

				_event.events = EPOLLIN | EPOLLET;
				_event.data.fd = _conn_sock;
				if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _conn_sock, &_event) == -1) {
					std::cerr << "Error: epoll_ctl failed" << std::endl;
					close(_conn_sock);
				}
			}
			else {
				HandleClient(_events[n].data.fd, servers);
			}
		}
	}
}

void Server::HandleClient(int client_fd, const std::vector<ServerConfig> &servers)
{
	int port = _client_port[client_fd];

	for (const auto &server : servers) {
		if (server.listen_port == port) {
			Request request(client_fd, server);
			request.ParseRequest();
		}
	}
}

Server::~Server() {}
