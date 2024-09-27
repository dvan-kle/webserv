#include "../include/Request.hpp"
#include "../include/WriteClient.hpp"

void Request::DeleteResponse() {
    // build the file path
    std::string fileToDelete = "uploads/" + _url;

    // check if the file exists and try to delete it
    if (std::remove(fileToDelete.c_str()) == 0) {
        // show file successfully deleted
        std::string successMessage = "<html><body><h1>File deleted successfully!</h1></body></html>";
        responseHeader(successMessage, HTTP_200);
    } else {
        // show file not found or unable to delete error
        std::cerr << "Error: File not found or unable to delete: " << fileToDelete << std::endl;
        std::string errorMessage = "<html><body><h1>404 Not Found</h1><p>The requested file was not found or could not be deleted.</p></body></html>";
        responseHeader(errorMessage, HTTP_404);
    }

    WriteClient::safeWriteToClient(_client_fd, _response);
}
