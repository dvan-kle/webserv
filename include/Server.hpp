#pragma once

#include "Libaries.hpp"
#include "JsonParser.hpp"

#define MAX_EVENTS 10

class Server
{
	private:
		std::vector<int> _socks;
		std::unordered_map<int, int> _client_port;
		//int _sock;
		struct sockaddr_in _address;
	
		int _epoll_fd;
		struct epoll_event _event;
		struct epoll_event _events[MAX_EVENTS];
		
		int _conn_sock;

		void EpollCreate();
		void CreateSocket(int port, const std::string &ip);
		
	public:
		Server(const std::vector<int> &ports, const std::vector<std::string> &hosts, const std::vector<ServerConfig> &servers);
		Server(const Server &src) = delete;
		Server &operator=(const Server &src) = delete;
		~Server();

		void EpollWait(const std::vector<ServerConfig> &servers);
		void HandleClient(int client_fd, const std::vector<ServerConfig> &servers);
};
