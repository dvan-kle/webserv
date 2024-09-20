#include "../include/WriteClient.hpp"

void WriteClient::safeWriteToClient(int client_fd, const std::string& response) {
    ssize_t bytes_written = write(client_fd, response.c_str(), response.size());
    if (bytes_written == -1) {
        std::cerr << "Error: write failed" << std::endl;
        close(client_fd);
        exit(EXIT_FAILURE); // Handle the error appropriately!!
    }
}
