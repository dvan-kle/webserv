#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <set>
#include <sys/epoll.h>
#include <netinet/in.h>
#include "Request.hpp" // Include for handling requests
#include <map>

#define MAX_EVENTS 10

struct ListeningSocket
{
    int 						sock_fd;
    std::string 				host;
    int 						port;
    std::vector<ServerConfig>	configs; // Ordered list; first is default
};

class ClientContext
{
	public:
		int			fd;
		std::string read_buffer;
		std::string write_buffer;
		bool		header_parsed;
		size_t		content_length;
		bool 		response_ready;
		int 		listening_socket_fd; // To identify which listening socket this client is associated with

		ClientContext() : fd(-1), header_parsed(false), content_length(0), response_ready(false), listening_socket_fd(-1) {}
		ClientContext(int client_fd, int listen_fd) : fd(client_fd), header_parsed(false), content_length(0), response_ready(false), listening_socket_fd(listen_fd) {}
};

class Server
{
	private:
		std::vector<ListeningSocket> _listening_sockets;
		std::unordered_map<int, ClientContext> _clients; // key: client_fd
		struct sockaddr_in _address;

		int _epoll_fd;
		struct epoll_event _event;
		struct epoll_event _events[MAX_EVENTS];

		// Socket Management
		void CreateListeningSockets(const std::vector<ServerConfig> &servers);
		void HandleDuplicateHostPort(ServerConfig &current, std::map<int, std::set<std::string>> &port_to_hostnames);
		void CreateAndBindSocket(ServerConfig &current);
		void InitializeSocketAddress(ServerConfig &current);
		void SetNonBlocking(int sock);

		// Epoll Management
		void EpollCreate();
		void EpollWait(const std::vector<ServerConfig> &servers);
		bool HandleEvent(int fd, uint32_t events, const std::vector<ServerConfig> &servers);

		// Client Connection Handling
		bool HandleListeningSocket(int fd);
		void AcceptConnection(int listening_fd);
		int AcceptClient(int listening_fd);
		void AddClientToEpoll(int client_fd);

		// Client I/O Handling
		void HandleClientRead(int client_fd, const std::vector<ServerConfig> &configs);
		void HandleClientWrite(int client_fd);
		ClientContext* GetClientContext(int client_fd);
		bool ReadClientData(int client_fd, ClientContext* client);
		bool IsFullRequestReceived(const ClientContext &client);
		void ProcessClientRequest(int client_fd, ClientContext* client, const std::vector<ServerConfig> &configs);

		// Client Closing
		void CloseClient(int client_fd);

		// Helper function to find the listening socket associated with a specific fd
		ListeningSocket* FindListeningSocket(int listening_socket_fd);

	public:
		Server(const std::vector<ServerConfig> &servers);
		Server(const Server &src) = delete;
		Server &operator=(const Server &src) = delete;
		~Server();
};
