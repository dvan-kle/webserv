#include "Server.hpp"
#include "Request.hpp"
#include "HeaderParser.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <cerrno>
#include <set>

// Utility function to compare host and port
bool compareHostPort(const ServerConfig &a, const ServerConfig &b) {
    return (a.listen_host == b.listen_host) && (a.listen_port == b.listen_port);
}

Server::Server(const std::vector<ServerConfig> &servers)
{
    CreateListeningSockets(servers);
    EpollCreate();
    std::cout << YELLOW << "Server is listening on addresses:" << BLUE << std::endl;
    for (size_t i = 0; i < _listening_sockets.size(); ++i) {
        std::cout << _listening_sockets[i].host << ":" << _listening_sockets[i].port << std::endl;
    }
    std::cout << RESET << std::endl;
    EpollWait(servers);
}

void Server::SetNonBlocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) {
        exit(EXIT_FAILURE);
    }
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        exit(EXIT_FAILURE);
    }
}

void Server::CreateListeningSockets(const std::vector<ServerConfig> &servers)
{
    std::set<int> used_ports;
    std::unordered_map<std::string, std::set<int>> hostname_port_map;  // Track hostnames per port

    std::vector<ServerConfig> remaining_servers = servers;

    while (!remaining_servers.empty()) {
        ServerConfig current = remaining_servers.front();
        remaining_servers.erase(remaining_servers.begin());

        std::string hostname_port_key = current.server_name + ":" + std::to_string(current.listen_port);

        // Check for duplicate port/hostname usage
        if (hostname_port_map[current.server_name].count(current.listen_port) > 0) {
            std::cerr << "Error: Duplicate server block for " << current.server_name 
                      << " on port " << current.listen_port << " already in use." << std::endl;
            exit(EXIT_FAILURE);
        }

        // Add hostname and port to map to track usage
        hostname_port_map[current.server_name].insert(current.listen_port);

        // Check if the port is already in use (but allow it for different hostnames)
        if (used_ports.find(current.listen_port) != used_ports.end()) {
            if (current.server_name.empty()) {
                std::cerr << "Error: Multiple server blocks are using port " << current.listen_port
                          << " without unique hostnames." << std::endl;
                exit(EXIT_FAILURE);
            }
        } else {
            // Mark the port as used
            used_ports.insert(current.listen_port);
        }

        // Continue setting up the socket as usual
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == -1) {
            exit(EXIT_FAILURE);
        }

        // Set socket options
        int opt = 1;
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
            close(sock);
            exit(EXIT_FAILURE);
        }

        // Set socket to non-blocking mode
        SetNonBlocking(sock);

        // Bind the socket to the host and port
        memset(&_address, 0, sizeof(_address));
        _address.sin_family = AF_INET;
        _address.sin_port = htons(current.listen_port);

        if (inet_pton(AF_INET, current.listen_host.c_str(), &_address.sin_addr) <= 0) {
            std::cerr << "Error: Invalid IP address " << current.listen_host << std::endl;
            close(sock);
            exit(EXIT_FAILURE);
        }

        if (bind(sock, (struct sockaddr*)&_address, sizeof(_address)) == -1) {
            std::cerr << RED << "Error: bind failed for " << current.listen_host 
                      << ":" << current.listen_port << RESET << std::endl;
            close(sock);
            exit(EXIT_FAILURE);
        }

        // Listen for incoming connections
        if (listen(sock, 4096) == -1) {
            close(sock);
            exit(EXIT_FAILURE);
        }

        // Store the listening socket information
        ListeningSocket ls;
        ls.sock_fd = sock;
        ls.host = current.listen_host;
        ls.port = current.listen_port;
        ls.configs.push_back(current);  // Add the current server config
        _listening_sockets.push_back(ls);
    }
}



void Server::EpollCreate()
{
    // Create epoll instance
    _epoll_fd = epoll_create1(0);
    if (_epoll_fd == -1) {
        exit(EXIT_FAILURE);
    }

    // Add the listening sockets to epoll
    for (size_t i = 0; i < _listening_sockets.size(); ++i) {
        ListeningSocket &ls = _listening_sockets[i];
        _event.events = EPOLLIN | EPOLLET;
        _event.data.fd = ls.sock_fd;
        if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, ls.sock_fd, &_event) == -1) {
            close(ls.sock_fd);
            exit(EXIT_FAILURE);
        }
    }
}

void Server::EpollWait(const std::vector<ServerConfig> &servers)
{
    while (true) {
        int nfds = epoll_wait(_epoll_fd, _events, MAX_EVENTS, -1);
        if (nfds == -1) {
            if (errno == EINTR)
                continue; // Interrupted by signal, retry
            exit(EXIT_FAILURE);
        }
        for (int n = 0; n < nfds; ++n) {
            int fd = _events[n].data.fd;
            uint32_t events = _events[n].events;

            // Check if it's a listening socket
            bool is_listening = false;
            ListeningSocket *listening_socket = NULL;
            for (size_t i = 0; i < _listening_sockets.size(); ++i) {
                if (_listening_sockets[i].sock_fd == fd) {
                    is_listening = true;
                    listening_socket = &_listening_sockets[i];
                    break;
                }
            }

            if (is_listening) {
                // Accept new connections
                while (true) {
                    struct sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    int client_fd = accept(fd, (struct sockaddr*)&client_addr, &client_len);
                    if (client_fd == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // No more connections to accept
                            break;
                        } else {
                            break;
                        }
                    }
                    // Set client socket to non-blocking
                    SetNonBlocking(client_fd);

                    // Add client socket to epoll
                    _event.events = EPOLLIN | EPOLLET;
                    _event.data.fd = client_fd;
                    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, client_fd, &_event) == -1) {
                        close(client_fd);
                        continue;
                    }

                    // Store client context
                    _clients.emplace(client_fd, ClientContext(client_fd, fd));

                    // Optionally, log the new connection
                    char client_ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
                }
            } else {
                // Handle client events
                if (events & EPOLLIN) {
                    HandleClientRead(fd, servers);
                }
                if (events & EPOLLOUT) {
                    HandleClientWrite(fd);
                }
                if (events & (EPOLLHUP | EPOLLERR)) {
                    CloseClient(fd);
                }
            }
        }
    }
}

void Server::HandleClientRead(int client_fd, const std::vector<ServerConfig> &configs) {
    auto it = _clients.find(client_fd);
    if (it == _clients.end()) {
        CloseClient(client_fd);
        return;
    }
    ClientContext &client = it->second;

    char buffer[4096];
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));
    
    if (bytes_read > 0) {
        // Successfully read some data
        client.read_buffer.append(buffer, bytes_read);
    } else if (bytes_read == 0) {
        // Client closed the connection
        CloseClient(client_fd);
        return;
    } else {
        // Error occurred in reading (since we can't use errno)
        CloseClient(client_fd);
        return;
    }

    // Check if the request headers have been fully received
    size_t pos = client.read_buffer.find("\r\n\r\n");
    if (pos != std::string::npos) {
        // Parse the Host header from the request and proceed...
        std::string host_header = HeaderParser::getHost(client.read_buffer);
        // Find the correct listening socket by matching the client's listening socket fd
        ListeningSocket* matched_socket = nullptr;
        for (size_t i = 0; i < _listening_sockets.size(); ++i) {
            if (_listening_sockets[i].sock_fd == client.listening_socket_fd) {
                matched_socket = &_listening_sockets[i];
                break;
            }
        }
        if (!matched_socket) {
            CloseClient(client_fd);
            return;
        }

        // Match the server configuration by host and port
        const ServerConfig* matched_config = nullptr;
        for (const ServerConfig &config : matched_socket->configs) {
            if (config.server_name == host_header) {
                matched_config = &config;
                break;
            }
        }
        if (!matched_config) {
            matched_config = &matched_socket->configs[0]; // Default configuration
        }

        // Handle request with the matched configuration
        std::vector<ServerConfig> matched_configs = {*matched_config};
        Request request(matched_configs, client.read_buffer);
        request.ParseRequest();

        client.write_buffer = request.getResponse();
        _event.events = EPOLLOUT | EPOLLET;
        _event.data.fd = client_fd;
        if (epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, client_fd, &_event) == -1) {
            CloseClient(client_fd);
        }
    }
}






void Server::HandleClientWrite(int client_fd)
{
    auto it = _clients.find(client_fd);
    if (it == _clients.end()) {
        CloseClient(client_fd);
        return;
    }
    ClientContext &client = it->second;

    if (client.write_buffer.empty()) {
        return;
    }

    ssize_t bytes_written = write(client_fd, client.write_buffer.c_str(), client.write_buffer.size());
    
    if (bytes_written > 0) {
        // Successfully wrote some data, remove that portion from the buffer
        client.write_buffer.erase(0, bytes_written);
    } else if (bytes_written == 0) {
        // Unusual case for write but treat it as the client closing the connection
        CloseClient(client_fd);
        return;
    } else {
        // Error occurred in writing (we do not rely on errno)
        CloseClient(client_fd);
        return;
    }

    // If all data has been written, close the client
    if (client.write_buffer.empty()) {
        CloseClient(client_fd);
    }
}
    


void Server::CloseClient(int client_fd)
{
    epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
    close(client_fd);
    _clients.erase(client_fd);
}

Server::~Server()
{
    for (size_t i = 0; i < _listening_sockets.size(); ++i) {
        close(_listening_sockets[i].sock_fd);
    }
    close(_epoll_fd);
}
