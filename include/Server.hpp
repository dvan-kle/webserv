#pragma once

#include "Libaries.hpp"
#include "JsonParser.hpp"
#include <unordered_map>
#include <vector>
#include <set>

#define MAX_EVENTS 10

struct ListeningSocket {
    int sock_fd;
    std::string host;
    int port;
    std::vector<ServerConfig> configs; // Ordered list; first is default
};

class ClientContext {
public:
    int fd;
    std::string read_buffer;
    std::string write_buffer;
    bool header_parsed;
    size_t content_length;
    bool response_ready;
    int listening_socket_fd; // To identify which listening socket this client is associated with

    // Default constructor
    ClientContext()
        : fd(-1), header_parsed(false), content_length(0), response_ready(false), listening_socket_fd(-1) {}

    // Constructor with client_fd and listening_socket_fd
    ClientContext(int client_fd, int listen_fd)
        : fd(client_fd), header_parsed(false), content_length(0), response_ready(false), listening_socket_fd(listen_fd) {}
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

    void EpollCreate();
    void CreateListeningSockets(const std::vector<ServerConfig> &servers);
    void SetNonBlocking(int sock);
    void HandleClientRead(int client_fd, const std::vector<ServerConfig> &servers);
    void HandleClientWrite(int client_fd);
    void CloseClient(int client_fd);

    void HandleClient(int client_fd, const std::vector<ServerConfig> &servers);

public:
    Server(const std::vector<ServerConfig> &servers);
    Server(const Server &src) = delete;
    Server &operator=(const Server &src) = delete;
    ~Server();

    void EpollWait(const std::vector<ServerConfig> &servers);
};

