#pragma once

#include "JsonParser.hpp"
#include <unordered_map>
#include <vector>
#include <set>
#include <sys/epoll.h>
#include <netinet/in.h>

#define MAX_EVENTS 10

struct ListeningSocket {
    int sock_fd;
    std::string host;
    int port;
    // List of server configurations associated with this socket
    std::vector<ServerConfig> configs;
};

class ClientContext {
    public:
        int fd;
        // Buffer for incoming data
        std::string read_buffer;
        // Buffer for outgoing data
        std::string write_buffer;
        bool header_parsed;
        size_t content_length;
        bool response_ready;
        // The listening socket this client is connected through
        int listening_socket_fd;

        // Default constructor
        ClientContext() : fd(-1), header_parsed(false), content_length(0), response_ready(false), listening_socket_fd(-1) {}

        // Constructor with client_fd and listening_socket_fd
        ClientContext(int client_fd, int listen_fd) : fd(client_fd), header_parsed(false), content_length(0), response_ready(false), listening_socket_fd(listen_fd) {}
};

class Server {
    private:
        // Stores all listening sockets
        std::vector<ListeningSocket> _listening_sockets;
        // Map of client_fd to client context
        std::unordered_map<int, ClientContext> _clients;
        struct sockaddr_in _address;

        int _epoll_fd;
        struct epoll_event _event;
        struct epoll_event _events[MAX_EVENTS];

        void EpollCreate();
        void CreateListeningSockets(const std::vector<ServerConfig> &servers);
        void SetNonBlocking(int sock);
        void HandleClientRead(int client_fd, const std::vector<ServerConfig> &servers);
        void HandleClientWrite(int client_fd);
        void CloseClient(int client_fd);

        void HandleClient(int client_fd, const std::vector<ServerConfig> &servers);

        bool HandleListeningSocket(int fd);
        void AcceptConnection(int listening_fd);


        ClientContext* GetClientContext(int client_fd);
        bool ReadClientData(int client_fd, ClientContext* client);
        bool IsFullRequestReceived(const ClientContext &client);
        ListeningSocket* FindListeningSocket(int listening_socket_fd);
        void ProcessClientRequest(int client_fd, ClientContext* client, const std::vector<ServerConfig> &configs);
    public:
        Server(const std::vector<ServerConfig> &servers);
        Server(const Server &src) = delete;
        Server &operator=(const Server &src) = delete;
        ~Server();

        void EpollWait(const std::vector<ServerConfig> &servers);
};
