#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <fcntl.h>
#include <stdlib.h>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#define PORT 8080
#define MAX_EVENTS 10

// void handle_client(int client_fd) {
//     char buffer[1024];
//     int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
//     if (bytes_read > 0) {
//         buffer[bytes_read] = '\0';
//         std::cout << "Received request:\n" << buffer << std::endl;

//         // Read the content of index.html
//         std::ifstream file("../www/index.html");
//         if (!file.is_open()) {
//             const char* not_found_response = "HTTP/1.1 404 Not Found\r\n"
//                                              "Content-Type: text/plain\r\n"
//                                              "Content-Length: 13\r\n"
//                                              "\r\n"
//                                              "404 Not Found";
//             write(client_fd, not_found_response, strlen(not_found_response));
//         } else {
//             std::stringstream file_buffer;
//             file_buffer << file.rdbuf();
//             std::string file_content = file_buffer.str();

//             std::stringstream http_response;
//             http_response << "HTTP/1.1 200 OK\r\n"
//                           << "Content-Type: text/html\r\n"
//                           << "Content-Length: " << file_content.size() << "\r\n"
//                           << "\r\n"
//                           << file_content;

//             std::string response = http_response.str();
//             write(client_fd, response.c_str(), response.size());
//         }
//     }
//     close(client_fd);
// }

void handle_client(int client_fd) {
    char buffer[1024];
    int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        std::cout << "Received request:\n" << buffer << std::endl;

        // Extract the requested file path from the HTTP request
        std::string request(buffer);
        std::string delimiter = " ";
        size_t pos = request.find(delimiter);
        size_t pos2 = request.find(delimiter, pos + 1);
        std::string file_path = request.substr(pos + 1, pos2 - pos - 1);
        if (file_path == "/") {
            file_path = "/index.html";
        }

        // Determine the MIME type
        std::string mime_type;
        if (file_path.ends_with(".html")) {
            mime_type = "text/html";
        } else if (file_path.ends_with(".css")) {
            mime_type = "text/css";
        } else {
            mime_type = "text/plain"; // default MIME type
        }

        // Open and read the requested file
        std::ifstream file("." + file_path);
        if (!file.is_open()) {
            const char* not_found_response = "HTTP/1.1 404 Not Found\r\n"
                                             "Content-Type: text/plain\r\n"
                                             "Content-Length: 13\r\n"
                                             "\r\n"
                                             "404 Not Found";
            write(client_fd, not_found_response, strlen(not_found_response));
        } else {
            std::stringstream file_buffer;
            file_buffer << file.rdbuf();
            std::string file_content = file_buffer.str();

            std::stringstream http_response;
            http_response << "HTTP/1.1 200 OK\r\n"
                          << "Content-Type: " << mime_type << "\r\n"
                          << "Content-Length: " << file_content.size() << "\r\n"
                          << "\r\n"
                          << file_content;

            std::string response = http_response.str();
            write(client_fd, response.c_str(), response.size());
        }
    }
    close(client_fd);
}


int main() {
    int listen_sock, conn_sock, epoll_fd;
    struct epoll_event ev, events[MAX_EVENTS];
    struct sockaddr_in addr;
    int addrlen = sizeof(addr);

    // Create listening socket
    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(listen_sock);
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the port
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    if (bind(listen_sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(listen_sock);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(listen_sock, 10) == -1) {
        perror("listen");
        close(listen_sock);
        exit(EXIT_FAILURE);
    }

    // Create epoll instance
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        close(listen_sock);
        exit(EXIT_FAILURE);
    }

    // Add the listening socket to epoll
    ev.events = EPOLLIN;
    ev.data.fd = listen_sock;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_sock, &ev) == -1) {
        perror("epoll_ctl: listen_sock");
        close(listen_sock);
        exit(EXIT_FAILURE);
    }

    std::cout << "Server is listening on port " << PORT << std::endl;

    // Event loop
    while (true) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("epoll_wait");
            close(listen_sock);
            exit(EXIT_FAILURE);
        }

        for (int n = 0; n < nfds; ++n) {
            if (events[n].data.fd == listen_sock) {
                // Handle new connection
                conn_sock = accept(listen_sock, (struct sockaddr*)&addr, (socklen_t*)&addrlen);
                if (conn_sock == -1) {
                    perror("accept");
                    continue;
                }
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = conn_sock;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_sock, &ev) == -1) {
                    perror("epoll_ctl: conn_sock");
                    close(conn_sock);
                    continue;
                }
            } else {
                // Handle client request
                handle_client(events[n].data.fd);
            }
        }
    }

    close(listen_sock);
    return 0;
}
