#include "../include/Request.hpp"

#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <iostream>

void Request::HandleDeleteRequest() {
    // retrieve the location configuration based on the requested URL
    auto location = findLocation(_url);
    
    // if location is not found, serve error
    if (location == nullptr) {
        std::cerr << "Error: Could not find location for URL: " << _url << std::endl;
        ServeErrorPage(500);
        return;
    }

    // ensure that the location has an upload path configured
    if (location->upload_path.empty()) {
        std::cerr << "Upload path not specified in the config for the current location!" << std::endl;
        ServeErrorPage(500);
        return;
    }

    // resolve the upload path to an absolute path
    std::string uploadPath = getAbsolutePath(location->upload_path);

    // ensure that the upload path ends with a slash ("/")
    if (uploadPath.back() != '/') {
        uploadPath += '/';
    }

    // check if the URL starts with the location's path
    if (_url.find(location->path) == 0) {
        // remove the location path from the URL
        _url = _url.substr(location->path.length());
    }

    // build the full file path by appending the URL to the upload path
    std::string fileToDelete = uploadPath + _url;

    // Check if the file exists
    struct stat buffer;
    if (stat(fileToDelete.c_str(), &buffer) != 0) {
        // Serve a 404 Not Found page if the file does not exist
        std::cerr << "Error: File not found: " << fileToDelete << std::endl;
        ServeErrorPage(404);
        return;
    }

    // attempt to delete the file
    if (std::remove(fileToDelete.c_str()) == 0) {
        // if file deletion succeeds, respond with a success message (200 OK)
        std::string successMessage = "<html><body><h1>File deleted successfully!</h1></body></html>";
        // set the response header for 200 OK
        responseHeader(successMessage, HTTP_200);
        // append the success message to the response
        _response += successMessage;
    } else {
        // if file deletion fails, serve a 500 Internal Server Error page
        std::cerr << "Error: Unable to delete file: " << fileToDelete << std::endl;
    	ServeErrorPage(500);
    }
}
