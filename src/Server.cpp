#include "../include/Server.hpp"
#include "../include/Request.hpp"
#include "../include/HeaderParser.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <cerrno>

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

    // Set socket to non-blocking mode
    SetNonBlocking(sock);

    // Bind the socket to the port
    memset(&_address, 0, sizeof(_address)); // Initialize the _address structure
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
        std::cout << BLUE << "  Check your IP address and port" << RESET << std::endl;
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

    // Add the listening sockets to epoll
    for (int sock : _socks) {
        _event.events = EPOLLIN | EPOLLOUT | EPOLLET;
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
            uint32_t events = _events[n].events;

            if (std::find(_socks.begin(), _socks.end(), fd) != _socks.end()) {
                // Accept new connections
                while (true) {
                    int client_fd = accept(fd, NULL, NULL);
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
                    _event.events = EPOLLIN | EPOLLOUT | EPOLLET;
                    _event.data.fd = client_fd;
                    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, client_fd, &_event) == -1) {
                        std::cerr << "Error: epoll_ctl failed" << std::endl;
                        close(client_fd);
                        continue;
                    }

                    // Store client context
                    _clients.emplace(client_fd, ClientContext(client_fd));

                    // Store the port the client is connected to
                    int server_port;
                    socklen_t len = sizeof(_address);
                    getsockname(client_fd, (struct sockaddr *)&_address, &len);
                    server_port = ntohs(_address.sin_port);

                    _client_port[client_fd] = server_port;
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

void Server::HandleClient(int client_fd, const std::vector<ServerConfig> &servers)
{
    int port = _client_port[client_fd];

    for (const auto &server : servers) {
        if (server.listen_port == port) {
            Request request(server, _clients[client_fd].read_buffer);
            request.ParseRequest();

            if (request.isResponseReady()) {
                _clients[client_fd].write_buffer = request.getResponse();
                // Modify epoll to watch for EPOLLOUT only
                _event.events = EPOLLOUT | EPOLLET;
                _event.data.fd = client_fd;
                if (epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, client_fd, &_event) == -1) {
                    std::cerr << "Error: epoll_ctl MOD failed" << std::endl;
                    CloseClient(client_fd);
                }
                break;
            }

            // If not a redirect, proceed normally
            _clients[client_fd].write_buffer = request.getResponse();

            // Modify epoll to watch for EPOLLOUT
            _event.events = EPOLLIN | EPOLLOUT | EPOLLET;
            _event.data.fd = client_fd;
            if (epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, client_fd, &_event) == -1) {
                std::cerr << "Error: epoll_ctl MOD failed" << std::endl;
                CloseClient(client_fd);
            }
            break;
        }
    }
}

Server::~Server() {}

void Server::HandleClientRead(int client_fd, const std::vector<ServerConfig> &servers)
{
    std::unordered_map<int, ClientContext>::iterator it = _clients.find(client_fd);
    if (it == _clients.end()) {
        // Handle error: client_fd not found in _clients
        std::cerr << "Error: client_fd not found in _clients" << std::endl;
        CloseClient(client_fd);
        return;
    }
    ClientContext &client = it->second;

    char buffer[1024];
    while (true) {
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            client.read_buffer.append(buffer, bytes_read);
        } else if (bytes_read == 0) {
            // Connection closed by client
            CloseClient(client_fd);
            return;
        } else {
            // Read would block or error occurred
            // Since we can't check errno, we assume that if bytes_read == -1, the operation would block
            break;
        }
    }

    // Check if the request headers have been fully received
    size_t pos = client.read_buffer.find("\r\n\r\n");
    if (pos != std::string::npos) {
        // Headers have been received; check for Content-Length
        size_t content_length = HeaderParser::getContentLength(client.read_buffer);
        if (client.read_buffer.size() >= pos + 4 + content_length) {
            // Full request has been received
            HandleClient(client_fd, servers);
        }
    }
}

void Server::HandleClientWrite(int client_fd)
{
    ClientContext &client = _clients[client_fd];

    if (client.write_buffer.empty()) {
        // No data to send
        return;
    }

    while (!client.write_buffer.empty()) {
        ssize_t bytes_written = write(client_fd, client.write_buffer.c_str(), client.write_buffer.size());
        if (bytes_written > 0) {
            client.write_buffer.erase(0, bytes_written);
        } else if (bytes_written == -1) {
            // Write would block or error occurred
            // Since we can't check errno, we assume that if bytes_written == -1, the operation would block
            break;
        }
    }

    if (client.write_buffer.empty()) {
        // Response has been fully sent
        CloseClient(client_fd); // Or reset client state for keep-alive
    }
}

void Server::CloseClient(int client_fd)
{
    epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
    close(client_fd);
    _clients.erase(client_fd);
    _client_port.erase(client_fd);
}
