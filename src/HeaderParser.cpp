#include "../include/HeaderParser.hpp"

std::pair<std::string, std::string> HeaderParser::readHeaders(int client_fd) {
    char buffer[1024];
    std::string headers;
    std::string body;
    int bytes_read;
    size_t total_bytes_read = 0;

    // Read headers from client
    while (true) {
        bytes_read = read(client_fd, buffer, sizeof(buffer));
        if (bytes_read == -1) {
            std::cerr << "Error: read failed" << std::endl;
            close(client_fd);
            exit(EXIT_FAILURE);
        } else if (bytes_read == 0) {
            break;
        }
        headers.append(buffer, bytes_read);
        total_bytes_read += bytes_read;

        size_t pos = headers.find("\r\n\r\n");
        if (pos != std::string::npos) {
            // Headers and body are separated by \r\n\r\n
            body = headers.substr(pos + 4); // Body starts after headers
            headers = headers.substr(0, pos + 4); // Headers section
            break;
        }
    }

    return std::make_pair(headers, body); // Return headers and body as a pair
}

size_t HeaderParser::getContentLength(const std::string& headers) {
    size_t cl_pos = headers.find("Content-Length:");
    if (cl_pos != std::string::npos) {
        size_t cl_end = headers.find("\r\n", cl_pos);
        std::string cl_str = headers.substr(cl_pos + 15, cl_end - (cl_pos + 15));
        return std::stoi(cl_str);
    }
    return 0;
}

bool HeaderParser::isChunkedEncoding(const std::string& headers) {
    return headers.find("Transfer-Encoding: chunked") != std::string::npos;
}
