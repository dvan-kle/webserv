#include "../include/Request.hpp"

void Request::DeleteResponse() {
    // build the file path
    std::string fileToDelete = "uploads/" + _url;

    // check if the file exists and try to delete it
    if (std::remove(fileToDelete.c_str()) == 0) {
        // show file successfully deleted
        std::string successMessage = "<html><body><h1>File deleted successfully!</h1></body></html>";
        _response += _http_version + " " + HTTP_200 + CONTYPE_HTML;
        _response += CONTENT_LENGTH + std::to_string(successMessage.size()) + "\r\n\r\n";
        _response += successMessage;
    } else {
        // show file not found or unable to delete error
        std::cerr << "Error: File not found or unable to delete: " << fileToDelete << std::endl;
        std::string errorMessage = "<html><body><h1>404 Not Found</h1><p>The requested file was not found or could not be deleted.</p></body></html>";
        _response += _http_version + " 404 Not Found\r\nContent-Type: text/html\r\n";
        _response += CONTENT_LENGTH + std::to_string(errorMessage.size()) + "\r\n\r\n";
        _response += errorMessage;
    }

    // send the response back to the client
    ssize_t bytes_written = write(_client_fd, _response.c_str(), _response.size());
    if (bytes_written == -1) {
        std::cerr << "Error: write failed" << std::endl;
        close(_client_fd);
        exit(EXIT_FAILURE);
    }
}
