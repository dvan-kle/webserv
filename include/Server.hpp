#pragma once

#include "Libaries.hpp"
#include "JsonParser.hpp"
#include <unordered_map>

#define MAX_EVENTS 10

class ClientContext {
public:
    int fd;
    std::string read_buffer;
    std::string write_buffer;
    bool header_parsed;
    size_t content_length;
    bool response_ready;
    // Additional state variables as needed

    // Default constructor
    ClientContext()
        : fd(-1), header_parsed(false), content_length(0), response_ready(false) {}

    // Constructor with client_fd
    ClientContext(int client_fd)
        : fd(client_fd), header_parsed(false), content_length(0), response_ready(false) {}
};

class Server
{
private:
    std::vector<int> _socks;
    std::unordered_map<int, int> _client_port;
    std::unordered_map<int, ClientContext> _clients;
    struct sockaddr_in _address;

    int _epoll_fd;
    struct epoll_event _event;
    struct epoll_event _events[MAX_EVENTS];

    void EpollCreate();
    void CreateSocket(int port, const std::string &ip);
    void SetNonBlocking(int sock);
    void HandleClientRead(int client_fd, const std::vector<ServerConfig> &servers);
    void HandleClientWrite(int client_fd);
    void CloseClient(int client_fd);

    void HandleClient(int client_fd, const std::vector<ServerConfig> &servers);

public:
    Server(const std::vector<int> &ports, const std::vector<std::string> &hosts, const std::vector<ServerConfig> &servers);
    Server(const Server &src) = delete;
    Server &operator=(const Server &src) = delete;
    ~Server();

    void EpollWait(const std::vector<ServerConfig> &servers);
};
