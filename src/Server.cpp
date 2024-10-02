#include "Server.hpp"
#include "Request.hpp"
#include "Header.hpp"
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



/* ------------------------ *\
|-----------Server-----------|
\* ------------------------ */

Server::Server(const std::vector<ServerConfig> &servers) {
    // create sockets for each server block in the config and bind them to their respective ports
    CreateListeningSockets(servers);

    // set up the epoll instance that will handle all I/O events for the server
    EpollCreate();

    // output all the addresses and ports the server is listening on
    std::cout << YELLOW << "Server is listening on addresses:" << BLUE << std::endl;
    for (size_t i = 0; i < _listening_sockets.size(); ++i) {
        std::cout << _listening_sockets[i].host << ":" << _listening_sockets[i].port << std::endl;
    }
    std::cout << RESET << std::endl;

    // enter the main loop to wait for events and process them
    EpollWait(servers);
}

Server::~Server() {
    // close all listening sockets
    for (size_t i = 0; i < _listening_sockets.size(); ++i) {
        close(_listening_sockets[i].sock_fd);
    }
    // close the epoll file descriptor
    close(_epoll_fd);
}



/* ---------------------------------------- *\
|-----------CreateListeningSockets-----------|
\* ---------------------------------------- */

void Server::CreateListeningSockets(const std::vector<ServerConfig> &servers) {
    std::set<std::pair<std::string, int>> used_port_host_pairs;
    std::map<int, std::set<std::string>> port_to_hostnames;
    std::vector<ServerConfig> remaining_servers = servers;

    while (!remaining_servers.empty()) {
        ServerConfig current = remaining_servers.front();
        remaining_servers.erase(remaining_servers.begin());

        std::pair<std::string, int> host_port_key = std::make_pair(current.listen_host, current.listen_port);

        if (used_port_host_pairs.find(host_port_key) != used_port_host_pairs.end()) {
            HandleDuplicateHostPort(current, port_to_hostnames);
        } else {
            used_port_host_pairs.insert(host_port_key);
            if (!current.server_name.empty()) {
                port_to_hostnames[current.listen_port].insert(current.server_name);
            }
            CreateAndBindSocket(current);
        }
    }
}

// Handles the case where a duplicate host/port is found
void Server::HandleDuplicateHostPort(ServerConfig &current, std::map<int, std::set<std::string>> &port_to_hostnames) {
    if (current.server_name.empty()) {
        std::cerr << RED << "Error: Multiple server blocks using port " << current.listen_port
                  << " and host " << current.listen_host << " without unique hostnames." << RESET << std::endl;
        exit(EXIT_FAILURE);
    }

    if (port_to_hostnames[current.listen_port].count(current.server_name) > 0) {
        std::cerr << RED << "Error: Duplicate server_name " << current.server_name
                  << " for port " << current.listen_port << RESET << std::endl;
        exit(EXIT_FAILURE);
    }

    port_to_hostnames[current.listen_port].insert(current.server_name);
}

// Create a socket, bind it, and add it to the listening sockets list
void Server::CreateAndBindSocket(ServerConfig &current) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) exit(EXIT_FAILURE);

    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        close(sock);
        exit(EXIT_FAILURE);
    }

    SetNonBlocking(sock);
    InitializeSocketAddress(current);

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

    ListeningSocket ls;
    ls.sock_fd = sock;
    ls.host = current.listen_host;
    ls.port = current.listen_port;
    ls.configs.push_back(current);
    _listening_sockets.push_back(ls);
}

// Initialize the socket address for binding
void Server::InitializeSocketAddress(ServerConfig &current) {
    memset(&_address, 0, sizeof(_address));
    _address.sin_family = AF_INET;
    _address.sin_port = htons(current.listen_port);

    if (inet_pton(AF_INET, current.listen_host.c_str(), &_address.sin_addr) <= 0) {
        std::cerr << RED << "Error: Invalid IP address " << current.listen_host << RESET << std::endl;
        exit(EXIT_FAILURE);
    }
}



/* ----------------------------- *\
|-----------EpollCreate-----------|
\* ----------------------------- */

void Server::EpollCreate() {
    // create an epoll instance
    _epoll_fd = epoll_create1(0);
    if (_epoll_fd == -1) {
        exit(EXIT_FAILURE);
    }

    // add all listening sockets to the epoll instance to monitor for incoming connections
    for (size_t i = 0; i < _listening_sockets.size(); ++i) {
        ListeningSocket &ls = _listening_sockets[i];
        // monitor for readable events with edge-triggered behavior
        _event.events = EPOLLIN | EPOLLET;

        _event.data.fd = ls.sock_fd;
        if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, ls.sock_fd, &_event) == -1) {
            close(ls.sock_fd);
            exit(EXIT_FAILURE);
        }
    }
}



/* --------------------------- *\
|-----------EpollWait-----------|
\* --------------------------- */

void Server::EpollWait(const std::vector<ServerConfig> &servers) {
    while (true) {
        int nfds = epoll_wait(_epoll_fd, _events, MAX_EVENTS, -1);
        if (nfds == -1) {
            if (errno == EINTR) continue;  // Restart loop if interrupted by a signal
            exit(EXIT_FAILURE);
        }

        for (int n = 0; n < nfds; ++n) {
            int fd = _events[n].data.fd;
            uint32_t events = _events[n].events;

            if (HandleEvent(fd, events, servers)) continue;
        }
    }
}

// Handle an epoll event (either from a listening socket or a client)
bool Server::HandleEvent(int fd, uint32_t events, const std::vector<ServerConfig> &servers) {
    if (HandleListeningSocket(fd)) return true;

    // Handle client I/O events
    if (events & EPOLLIN) {
        HandleClientRead(fd, servers);
    }
    if (events & EPOLLOUT) {
        HandleClientWrite(fd);
    }
    if (events & (EPOLLHUP | EPOLLERR)) {
        CloseClient(fd);
    }
    return false;
}

bool Server::HandleListeningSocket(int fd) {
    // check if the event is on one of the listening sockets and accept new clients
    for (size_t i = 0; i < _listening_sockets.size(); ++i) {
        if (_listening_sockets[i].sock_fd == fd) {
            AcceptConnection(fd);
            return true;
        }
    }
    // the event is not on a listening socket
    return false;
}



/* ---------------------------- *\
|-----------ClientRead-----------|
\* ---------------------------- */

void Server::HandleClientRead(int client_fd, const std::vector<ServerConfig> &configs) {
    // Find client context and check for validity
    ClientContext* client = GetClientContext(client_fd);
    if (!client) return;

    // Read incoming data from the client into the read buffer
    if (!ReadClientData(client_fd, client)) return;

    // Check if the full request has been received (headers and body)
    if (IsFullRequestReceived(*client)) {
        // Process the request and prepare the response
        ProcessClientRequest(client_fd, client, configs);
    }
}

// Helper function to find and return the client context
ClientContext* Server::GetClientContext(int client_fd) {
    auto it = _clients.find(client_fd);
    if (it == _clients.end()) {
        CloseClient(client_fd);  // Close connection if client not found
        return nullptr;
    }
    return &it->second;  // Return the client context
}

// Helper function to read incoming data from the client
bool Server::ReadClientData(int client_fd, ClientContext* client) {
    char buffer[4096];
    while (true) {
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));
        
        if (bytes_read > 0) {
            // Append data to the client's read buffer
            client->read_buffer.append(buffer, bytes_read);
        } else if (bytes_read == 0) {
            // Client closed the connection
            CloseClient(client_fd);
            return false;
        } else if (bytes_read < 0) {
            // No more data to read at the moment
            break;
        }
    }
    return true;  // Successfully read data
}

// Helper function to check if the full request has been received (headers and body)
bool Server::IsFullRequestReceived(const ClientContext &client) {
    size_t header_end_pos = client.read_buffer.find("\r\n\r\n");
    if (header_end_pos != std::string::npos) {
        size_t content_length = Header::getContentLength(client.read_buffer);
        return client.read_buffer.size() >= header_end_pos + 4 + content_length;
    }
    return false;  // Full request not received yet
}

ListeningSocket* Server::FindListeningSocket(int listening_socket_fd) {
    // locate the listening socket by its file descriptor
    for (size_t i = 0; i < _listening_sockets.size(); ++i) {
        if (_listening_sockets[i].sock_fd == listening_socket_fd) {
            return &_listening_sockets[i];
        }
    }
    return nullptr;
}

// Helper function to process the client's request and prepare the response
void Server::ProcessClientRequest(int client_fd, ClientContext* client, const std::vector<ServerConfig> &configs) {
    ListeningSocket* matched_socket = FindListeningSocket(client->listening_socket_fd);
    if (!matched_socket) {
        // if no matching socket is found, close the connection
        CloseClient(client_fd);
        return;
    }

    // get the correct port associated with the socket
    int port = matched_socket->port;
    // create request object with config and port
    Request request(configs, client->read_buffer, port);
    // parse the request headers and body
    request.ParseRequest();

    // store the response in the write buffer for the client
    client->write_buffer = request.getResponse();

    // modify the epoll event to wait for the socket to be ready to write the response
    _event.events = EPOLLOUT | EPOLLET;
    _event.data.fd = client_fd;
    if (epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, client_fd, &_event) == -1) {
        // if epoll modification fails, close the connection
        CloseClient(client_fd);
    }
}



/* ----------------------------- *\
|-----------ClientWrite-----------|
\* ----------------------------- */

void Server::HandleClientWrite(int client_fd) {
    // find the client context by file descriptor
    auto it = _clients.find(client_fd);
    if (it == _clients.end()) {
        CloseClient(client_fd);
        return;
    }
    ClientContext &client = it->second;

    // if there's nothing to write, return
    if (client.write_buffer.empty())
        return;

    // write the response to the client
    ssize_t bytes_written = write(client_fd, client.write_buffer.c_str(), client.write_buffer.size());

    if (bytes_written > 0) {
        // remove the written part from the buffer
        client.write_buffer.erase(0, bytes_written);
    }

    // if the write buffer is empty, close the connection
    if (client.write_buffer.empty()) {
        CloseClient(client_fd);
    }
}



/* ---------------------------------- *\
|-----------AcceptConnection-----------|
\* ---------------------------------- */

void Server::AcceptConnection(int listening_fd) {
    while (true) {
        int client_fd = AcceptClient(listening_fd);
        if (client_fd == -1) return;  // Stop accepting clients if no more are available

        SetNonBlocking(client_fd);
        AddClientToEpoll(client_fd);
        _clients.emplace(client_fd, ClientContext(client_fd, listening_fd));
    }
}

// Accept a client connection and return the file descriptor
int Server::AcceptClient(int listening_fd) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(listening_fd, (struct sockaddr*)&client_addr, &client_len);
    
    if (client_fd == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -1;  // No more clients to accept
        } else {
            std::cerr << RED << "Error: accept() failed." << RESET << std::endl;
            return -1;
        }
    }
    return client_fd;
}

// Add the client file descriptor to the epoll instance
void Server::AddClientToEpoll(int client_fd) {
    _event.events = EPOLLIN | EPOLLET;
    _event.data.fd = client_fd;
    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, client_fd, &_event) == -1) {
        close(client_fd);
        std::cerr << RED << "Error: Failed to add client to epoll." << RESET << std::endl;
    }
}



/* ----------------------------- *\
|-----------CloseClient-----------|
\* ----------------------------- */

void Server::CloseClient(int client_fd) {
    // remove the client from epoll monitoring and close the connection
    epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
    close(client_fd);
    _clients.erase(client_fd);
}



/* -------------------------------- *\
|-----------SetNonBlocking-----------|
\* -------------------------------- */

void Server::SetNonBlocking(int sock) {
    // retrieve the current flags on the socket
    int flags = fcntl(sock, F_GETFL, 0);
    // exit if there's an error
    if (flags == -1) {
        exit(EXIT_FAILURE);
    }

    // set the socket to non-blocking mode
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        exit(EXIT_FAILURE);
    }
}
