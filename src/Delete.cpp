#include "../include/Request.hpp"

#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <iostream>

// Helper function to get absolute path from a relative path
std::string getAbsolutePath2(const std::string &path) {
    char absPath[PATH_MAX];
    
    // If the path is already absolute, return it
    if (path[0] == '/') {
        return path;
    }

    // Otherwise, treat it as relative to the current working directory
    if (realpath(path.c_str(), absPath) == nullptr) {
        std::cerr << "Error resolving absolute path: " << path << std::endl;
        return path;  // Return the original path if we can't resolve the absolute path
    }
    
    return std::string(absPath);
}

void Request::DeleteResponse() {
    // Retrieve the upload path from the location configuration
    auto location = findLocation(_url);
    
    if (location == nullptr) {
        std::cerr << "Error: Could not find location for URL: " << _url << std::endl;
        ServeErrorPage(500);  // Internal Server Error
        return;
    }

    if (location->upload_path.empty()) {
        std::cerr << "Upload path not specified in the config for the current location!" << std::endl;
        ServeErrorPage(500);  // Internal Server Error
        return;
    }

    // Resolve the upload path to an absolute path
    std::string uploadPath = getAbsolutePath2(location->upload_path);

    // Ensure the upload path and URL are concatenated correctly
    if (uploadPath.back() != '/') {
        uploadPath += '/';
    }

    // Check if the URL starts with the location path
    if (_url.find(location->path) == 0) {
        // Remove the location path prefix from _url
        _url = _url.substr(location->path.length());
    }

    // Build the full file path
    std::string fileToDelete = uploadPath + _url;

    // Check if the file exists
    struct stat buffer;
    if (stat(fileToDelete.c_str(), &buffer) != 0) {
        std::cerr << "Error: File not found: " << fileToDelete << std::endl;
        std::string errorMessage = "<html><body><h1>404 Not Found</h1><p>The requested file was not found.</p></body></html>";
        responseHeader(errorMessage, HTTP_404);  // Use responseHeader for error response
        _response += errorMessage;
        return;
    }

    if (std::remove(fileToDelete.c_str()) == 0) {
        // File successfully deleted
        std::string successMessage = "<html><body><h1>File deleted successfully!</h1></body></html>";
        responseHeader(successMessage, HTTP_200);
        _response += successMessage;
    } else {
        // Show file not found or unable to delete error
        std::cerr << "Error: Unable to delete file: " << fileToDelete << std::endl;
        std::string errorMessage = "<html><body><h1>500 Internal Server Error</h1><p>Unable to delete the requested file.</p></body></html>";
        responseHeader(errorMessage, HTTP_500);
        _response += errorMessage;
    }

    // The response is stored in _response and will be sent by the server
}
