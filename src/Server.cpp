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
#include <algorithm>

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
        std::cerr << "Error: fcntl F_GETFL failed" << std::endl;
        exit(EXIT_FAILURE);
    }
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        std::cerr << "Error: fcntl F_SETFL failed" << std::endl;
        exit(EXIT_FAILURE);
    }
}

void Server::CreateListeningSockets(const std::vector<ServerConfig> &servers)
{
    // Group servers by listen_host and listen_port
    std::vector<ServerConfig> remaining_servers = servers;

    while (!remaining_servers.empty()) {
        ServerConfig current = remaining_servers.front();
        remaining_servers.erase(remaining_servers.begin());

        // Find all servers with the same host and port
        std::vector<ServerConfig> grouped_servers;
        grouped_servers.push_back(current);

        auto it = remaining_servers.begin();
        while (it != remaining_servers.end()) {
            if (compareHostPort(current, *it)) {
                grouped_servers.push_back(*it);
                it = remaining_servers.erase(it);
            } else {
                ++it;
            }
        }

        // Create socket for this host and port
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == -1) {
            std::cerr << "Error: socket creation failed" << std::endl;
            exit(EXIT_FAILURE);
        }

        // Set socket options
        int opt = 1;
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
            std::cerr << "Error: setsockopt failed" << std::endl;
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
            std::cerr << RED << "Error: bind failed for " << current.listen_host << ":" << current.listen_port << RESET << std::endl;
            std::cout << BLUE << "  Check your IP address and port" << RESET << std::endl;
            close(sock);
            exit(EXIT_FAILURE);
        }

        // Listen for incoming connections
        if (listen(sock, 4096) == -1) {
            std::cerr << "Error: listen failed for " << current.listen_host << ":" << current.listen_port << std::endl;
            close(sock);
            exit(EXIT_FAILURE);
        }

        // Store the listening socket information
        ListeningSocket ls;
        ls.sock_fd = sock;
        ls.host = current.listen_host;
        ls.port = current.listen_port;
        ls.configs = grouped_servers; // First server is default

        _listening_sockets.push_back(ls);
    }
}

void Server::EpollCreate()
{
    // Create epoll instance
    _epoll_fd = epoll_create1(0);
    if (_epoll_fd == -1) {
        std::cerr << "Error: epoll_create1 failed" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Add the listening sockets to epoll
    for (size_t i = 0; i < _listening_sockets.size(); ++i) {
        ListeningSocket &ls = _listening_sockets[i];
        _event.events = EPOLLIN | EPOLLET;
        _event.data.fd = ls.sock_fd;
        if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, ls.sock_fd, &_event) == -1) {
            std::cerr << "Error: epoll_ctl ADD failed for listening socket " << ls.sock_fd << std::endl;
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
            std::cerr << "Error: epoll_wait failed" << std::endl;
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
                            std::cerr << "Error: accept failed" << std::endl;
                            break;
                        }
                    }
                    // Set client socket to non-blocking
                    SetNonBlocking(client_fd);

                    // Add client socket to epoll
                    _event.events = EPOLLIN | EPOLLET;
                    _event.data.fd = client_fd;
                    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, client_fd, &_event) == -1) {
                        std::cerr << "Error: epoll_ctl ADD failed for client " << client_fd << std::endl;
                        close(client_fd);
                        continue;
                    }

                    // Store client context
                    _clients.emplace(client_fd, ClientContext(client_fd, fd));

                    // Optionally, log the new connection
                    char client_ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
                    std::cout << "Accepted connection from " << client_ip << ":" << ntohs(client_addr.sin_port) << " (fd: " << client_fd << ")" << std::endl;
                }
            } else {
                // Handle client events
                if (events & EPOLLIN) {
                    // Find which listening socket this client is associated with
                    std::unordered_map<int, ClientContext>::iterator it = _clients.find(fd);
                    if (it == _clients.end()) {
                        std::cerr << "Error: client_fd not found in _clients" << std::endl;
                        CloseClient(fd);
                        continue;
                    }
                    ClientContext &client = it->second;

                    // Find the listening socket based on listening_socket_fd
                    ListeningSocket *ls = NULL;
                    for (size_t i = 0; i < _listening_sockets.size(); ++i) {
                        if (_listening_sockets[i].sock_fd == client.listening_socket_fd) {
                            ls = &_listening_sockets[i];
                            break;
                        }
                    }
                    if (!ls) {
                        std::cerr << "Error: Listening socket not found for client " << fd << std::endl;
                        CloseClient(fd);
                        continue;
                    }

                    HandleClientRead(fd, ls->configs);
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

void Server::HandleClientRead(int client_fd, const std::vector<ServerConfig> &configs)
{
    std::unordered_map<int, ClientContext>::iterator it = _clients.find(client_fd);
    if (it == _clients.end()) {
        // Handle error: client_fd not found in _clients
        std::cerr << "Error: client_fd not found in _clients" << std::endl;
        CloseClient(client_fd);
        return;
    }
    ClientContext &client = it->second;

    char buffer[4096];
    while (true) {
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            client.read_buffer.append(buffer, bytes_read);
        } else if (bytes_read == 0) {
            // Connection closed by client
            CloseClient(client_fd);
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No more data to read
                break;
            } else {
                std::cerr << "Error: read failed for client " << client_fd << std::endl;
                CloseClient(client_fd);
                return;
            }
        }
    }

    // Check if the request headers have been fully received
    size_t pos = client.read_buffer.find("\r\n\r\n");
    if (pos != std::string::npos) {
        // Headers have been received; check for Content-Length
        size_t content_length = HeaderParser::getContentLength(client.read_buffer);
        if (client.read_buffer.size() >= pos + 4 + content_length) {
            // Full request has been received
            // Create a Request object and parse
            Request request(configs, client.read_buffer);
            request.ParseRequest();

            if (request.isResponseReady()) {
                client.write_buffer = request.getResponse();
                // Modify epoll to watch for EPOLLOUT only
                _event.events = EPOLLOUT | EPOLLET;
                _event.data.fd = client_fd;
                if (epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, client_fd, &_event) == -1) {
                    std::cerr << "Error: epoll_ctl MOD failed for client " << client_fd << std::endl;
                    CloseClient(client_fd);
                }
                return;
            }

            // If response is not ready yet, keep EPOLLIN and EPOLLOUT
            client.write_buffer = request.getResponse();
            _event.events = EPOLLIN | EPOLLOUT | EPOLLET;
            _event.data.fd = client_fd;
            if (epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, client_fd, &_event) == -1) {
                std::cerr << "Error: epoll_ctl MOD failed for client " << client_fd << std::endl;
                CloseClient(client_fd);
            }
        }
    }
}

void Server::HandleClientWrite(int client_fd)
{
    std::unordered_map<int, ClientContext>::iterator it = _clients.find(client_fd);
    if (it == _clients.end()) {
        std::cerr << "Error: client_fd not found in _clients" << std::endl;
        CloseClient(client_fd);
        return;
    }
    ClientContext &client = it->second;

    if (client.write_buffer.empty()) {
        // No data to send
        return;
    }

    while (!client.write_buffer.empty()) {
        ssize_t bytes_written = write(client_fd, client.write_buffer.c_str(), client.write_buffer.size());
        if (bytes_written > 0) {
            client.write_buffer.erase(0, bytes_written);
        } else if (bytes_written == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Cannot write more now
                break;
            } else {
                std::cerr << "Error: write failed for client " << client_fd << std::endl;
                CloseClient(client_fd);
                return;
            }
        }
    }

    if (client.write_buffer.empty()) {
        // Response has been fully sent
        // Optionally, implement keep-alive by resetting read_buffer and write_buffer
        // For simplicity, we'll close the connection
        CloseClient(client_fd);
    }
}

void Server::CloseClient(int client_fd)
{
    epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
    close(client_fd);
    _clients.erase(client_fd);
    std::cout << "Closed connection with client fd: " << client_fd << std::endl;
}

Server::~Server()
{
    for (size_t i = 0; i < _listening_sockets.size(); ++i) {
        close(_listening_sockets[i].sock_fd);
    }
    close(_epoll_fd);
}
