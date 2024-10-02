#include "Server.hpp"
#include "Request.hpp"
#include "HeaderParser.hpp"
#include "Colors.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <cerrno>
#include <set>
#include <map>

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
    std::set<std::pair<std::string, int>> used_port_host_pairs;  // Track used host:port combinations
    std::map<int, std::set<std::string>> port_to_hostnames;      // Map from port to a set of hostnames

    std::vector<ServerConfig> remaining_servers = servers;

    while (!remaining_servers.empty()) {
        ServerConfig current = remaining_servers.front();
        remaining_servers.erase(remaining_servers.begin());

        std::pair<std::string, int> host_port_key = std::make_pair(current.listen_host, current.listen_port);

        // Check if the port and host combination is already in use
        if (used_port_host_pairs.find(host_port_key) != used_port_host_pairs.end()) {
            // If the port and host are in use, check the server_name logic
            if (current.server_name.empty()) {
                // If the current server has no server_name, it's an error (port + host without server_name already used)
                std::cerr << RED << "Error: Multiple server blocks are using port " << current.listen_port
                          << " and host " << current.listen_host << " without unique hostnames." << RESET << std::endl;
                exit(EXIT_FAILURE);
            }

            // Check if the same server_name is also already in use for this port
            if (port_to_hostnames[current.listen_port].count(current.server_name) > 0) {
                std::cerr << RED << "Error: Duplicate server_name " << current.server_name
                          << " for port " << current.listen_port << " and host " << current.listen_host << RESET << std::endl;
                exit(EXIT_FAILURE);
            }

            // Otherwise, add the server_name to the set of hostnames for this port
            port_to_hostnames[current.listen_port].insert(current.server_name);
        } else {
            // Mark the host:port as used
            used_port_host_pairs.insert(host_port_key);

            // If there's a server_name, register it to the port
            if (!current.server_name.empty()) {
                port_to_hostnames[current.listen_port].insert(current.server_name);
            }

            // Create the socket for this host and port
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock == -1) {
                exit(EXIT_FAILURE);
            }

            int opt = 1;
            if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
                close(sock);
                exit(EXIT_FAILURE);
            }

            SetNonBlocking(sock);

            memset(&_address, 0, sizeof(_address));
            _address.sin_family = AF_INET;
            _address.sin_port = htons(current.listen_port);

            if (inet_pton(AF_INET, current.listen_host.c_str(), &_address.sin_addr) <= 0) {
                std::cerr << RED << "Error: Invalid IP address " << current.listen_host << RESET << std::endl;
                close(sock);
                exit(EXIT_FAILURE);
            }

            if (bind(sock, (struct sockaddr*)&_address, sizeof(_address)) == -1) {
                std::cerr << RED << "Error: bind failed for " << current.listen_host 
                          << ":" << current.listen_port << RESET << std::endl;
                close(sock);
                exit(EXIT_FAILURE);
            }

            if (listen(sock, 4096) == -1) {
                close(sock);
                exit(EXIT_FAILURE);
            }

            // Store the listening socket information
            ListeningSocket ls;
            ls.sock_fd = sock;
            ls.host = current.listen_host;
            ls.port = current.listen_port;
            ls.configs.push_back(current);
            _listening_sockets.push_back(ls);
        }
    }
}


void Server::EpollCreate()
{
    _epoll_fd = epoll_create1(0);
    if (_epoll_fd == -1) {
        exit(EXIT_FAILURE);
    }

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
            if (errno == EINTR) continue;
            exit(EXIT_FAILURE);
        }
        for (int n = 0; n < nfds; ++n) {
            int fd = _events[n].data.fd;
            uint32_t events = _events[n].events;

            if (HandleListeningSocket(fd)) continue;

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

bool Server::HandleListeningSocket(int fd)
{
    for (size_t i = 0; i < _listening_sockets.size(); ++i) {
        if (_listening_sockets[i].sock_fd == fd) {
            AcceptConnection(fd);
            return true;
        }
    }
    return false;
}

void Server::AcceptConnection(int listening_fd)
{
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listening_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            else return;
        }

        SetNonBlocking(client_fd);
        _event.events = EPOLLIN | EPOLLET;
        _event.data.fd = client_fd;
        if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, client_fd, &_event) == -1) {
            close(client_fd);
            continue;
        }

        _clients.emplace(client_fd, ClientContext(client_fd, listening_fd));
    }
}

// Modified HandleClientRead function without using errno
void Server::HandleClientRead(int client_fd, const std::vector<ServerConfig> &configs)
{
    auto it = _clients.find(client_fd);
    if (it == _clients.end()) {
        CloseClient(client_fd);
        return;
    }
    ClientContext &client = it->second;

    char buffer[4096];
    while (true) {
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));

        // Success case: append the read data to the client's read buffer
        if (bytes_read > 0) {
            client.read_buffer.append(buffer, bytes_read);
        }
        // Connection closed by the client
        else if (bytes_read == 0) {
            CloseClient(client_fd);
            return;
        }
        // Handle the case where no more data is available, break the loop
        else if (bytes_read < 0) {
            // No need to check errno; assume this means no more data is available right now
            break;
        }
    }

    // Detect the end of headers
    size_t header_end_pos = client.read_buffer.find("\r\n\r\n");
    if (header_end_pos != std::string::npos) {
        size_t content_length = HeaderParser::getContentLength(client.read_buffer);
        if (client.read_buffer.size() >= header_end_pos + 4 + content_length) {

            ListeningSocket* matched_socket = FindListeningSocket(client.listening_socket_fd);
            if (!matched_socket) {
                CloseClient(client_fd);
                return;
            }

            int port = matched_socket->port;  // Pass the correct port

            Request request(configs, client.read_buffer, port);  // Pass port to Request
            request.ParseRequest();

            client.write_buffer = request.getResponse();
            _event.events = EPOLLOUT | EPOLLET;
            _event.data.fd = client_fd;
            if (epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, client_fd, &_event) == -1) {
                CloseClient(client_fd);
            }
        }
    }
}

// Modified HandleClientWrite function without using errno
void Server::HandleClientWrite(int client_fd)
{
    auto it = _clients.find(client_fd);
    if (it == _clients.end()) {
        CloseClient(client_fd);
        return;
    }
    ClientContext &client = it->second;

    if (client.write_buffer.empty()) {
        return; // Nothing to write, skip
    }

    ssize_t bytes_written = write(client_fd, client.write_buffer.c_str(), client.write_buffer.size());

    // Success case: adjust the write buffer by removing written bytes
    if (bytes_written > 0) {
        client.write_buffer.erase(0, bytes_written);
    }
    // Handle the case where no data is written, meaning the socket is not ready for writing
    else if (bytes_written < 0) {
        // If write fails, assume the socket isn't ready; epoll will tell us when to try again
        return;
    }

    // If the buffer is empty after writing, close the connection
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

ListeningSocket* Server::FindListeningSocket(int listening_socket_fd) {
    for (size_t i = 0; i < _listening_sockets.size(); ++i) {
        if (_listening_sockets[i].sock_fd == listening_socket_fd) {
            return &_listening_sockets[i];  // Return the matched listening socket
        }
    }
    return nullptr;
}


const ServerConfig* Server::FindMatchedConfig(ListeningSocket* listening_socket, const std::string &host)
{
    for (const ServerConfig &config : listening_socket->configs) {
        if (config.server_name == host) {
            return &config;
        }
    }
    return nullptr;
}

Server::~Server()
{
    for (size_t i = 0; i < _listening_sockets.size(); ++i) {
        close(_listening_sockets[i].sock_fd);
    }
    close(_epoll_fd);
}
