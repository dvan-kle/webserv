#include "../include/Request.hpp"
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>  // For PATH_MAX
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cctype>


// Helper function to get absolute path from a relative or absolute one
std::string getAbsolutePath(const std::string &path) {
    char absPath[PATH_MAX];

    // If the path starts with "/", it's already absolute, return it as is
    if (path[0] == '/') {
        return path;
    }

    // Otherwise, treat it as relative to the current working directory
    if (realpath(".", absPath) == nullptr) {
        std::cerr << "Error resolving current working directory!" << std::endl;
        return path;  // Return the original path if we can't resolve the current directory
    }

    // Return the combined absolute path for relative paths
    return std::string(absPath) + "/" + path;
}

size_t parseBodySize(const std::string& size_str) {
    size_t multiplier = 1;
    std::string number_part;
    
    // Get the numeric part
    for (char c : size_str) {
        if (std::isdigit(c)) {
            number_part += c;
        } else {
            break;
        }
    }

    // Parse the numeric part
    size_t body_size = std::stoull(number_part);

    // Determine the multiplier (K, M, G)
    if (size_str.find("K") != std::string::npos) {
        multiplier = 1000;  // Kilobytes
    } else if (size_str.find("M") != std::string::npos) {
        multiplier = 1000 * 1000;  // Megabytes
    } else if (size_str.find("G") != std::string::npos) {
        multiplier = 1000 * 1000 * 1000;  // Gigabytes
    }

    return body_size * multiplier;
}

void Request::PostResponse(const std::string &requestBody) {
    std::string contentType;
    std::string line;
    std::istringstream headerStream(_headers);

    // Convert the max body size from config
    size_t maxBodySize = parseBodySize(_config.client_max_body_size);

    // Check Content-Length header
    size_t contentLength = 0;
    size_t contentLengthPos = _headers.find("Content-Length:");
    if (contentLengthPos != std::string::npos) {
        size_t contentLengthEnd = _headers.find("\r\n", contentLengthPos);
        std::string contentLengthStr = _headers.substr(contentLengthPos + 15, contentLengthEnd - (contentLengthPos + 15));
        contentLength = std::stoull(contentLengthStr);
    }

    // Check if body size exceeds the limit set in the config
    if (contentLength > maxBodySize) {
        std::cerr << "Request body too large: " << contentLength << " bytes (Max allowed: " << maxBodySize << " bytes)" << std::endl;
        ServeErrorPage(413);
        return;
    }

    // Extract Content-Type from headers
    while (std::getline(headerStream, line) && line != "\r") {
        if (line.find("Content-Type:") != std::string::npos) {
            size_t ctStart = line.find("Content-Type:") + 13;
            contentType = line.substr(ctStart);
            // Remove any trailing whitespace or line breaks
            contentType.erase(contentType.find_last_not_of(" \t\r\n") + 1);
            break;
        }
    }

    // Normalize the content type (lowercase, remove whitespaces)
    std::transform(contentType.begin(), contentType.end(), contentType.begin(), ::tolower);
    contentType.erase(std::remove_if(contentType.begin(), contentType.end(), ::isspace), contentType.end());

    // Handle common content types
    if (contentType == "application/x-www-form-urlencoded") {
        std::cout << "Processing application/x-www-form-urlencoded data..." << std::endl;
        handleFormUrlEncoded(requestBody);
    } 
    else if (contentType.find("multipart/form-data") != std::string::npos) {
        std::cout << "Processing multipart/form-data data..." << std::endl;
        handleMultipartFormData(requestBody);
    } 
    else if (contentType == "text/plain" || contentType == "plain/text") {
        std::cout << "Processing text/plain data..." << std::endl;
        handlePlainText(requestBody);
    }
    else if (contentType == "application/json") {
        std::cout << "Processing application/json data..." << std::endl;
        handleJson(requestBody);
    }
    else {
        std::cerr << "Unsupported content type: " << contentType << std::endl;
        handleUnsupportedContentType();
    }
}

// Handling for x-www-form-urlencoded
void Request::handleFormUrlEncoded(const std::string &requestBody) {
    std::istringstream bodyStream(requestBody);
    std::string keyValue;
    std::map<std::string, std::string> formData;

    while (std::getline(bodyStream, keyValue, '&')) {
        size_t pos = keyValue.find('=');
        if (pos != std::string::npos) {
            std::string key = keyValue.substr(0, pos);
            std::string value = keyValue.substr(pos + 1);
            formData[key] = value;
        }
    }

    // Log key-value pairs
    for (const auto &pair : formData) {
        std::cout << "Key: " << pair.first << ", Value: " << pair.second << std::endl;
    }

    // Create a response
    std::string htmlContent = "<html><body><h1>Form data received successfully!</h1></body></html>";
    sendHtmlResponse(htmlContent);
}

// Handling for plain text
void Request::handlePlainText(const std::string &requestBody) {
    std::cout << "Received plain text: " << requestBody << std::endl;

    std::string responseText = "Received plain text: " + requestBody;
    std::string htmlContent = "<html><body><h1>" + responseText + "</h1></body></html>";
    sendHtmlResponse(htmlContent);
}

// Handling for JSON
void Request::handleJson(const std::string &requestBody) {
    std::cout << "Received JSON data: " << requestBody << std::endl;

    // Example: Process JSON data (could use a JSON parser here)
    std::string htmlContent = "<html><body><h1>Received JSON data!</h1></body></html>";
    sendHtmlResponse(htmlContent);
}

void Request::handleMultipartFormData(const std::string &requestBody) {
    // Extract boundary from headers
    std::string boundary;
    std::string line;
    std::istringstream headerStream(_headers);

    // Retrieve the upload path from the location configuration
    auto location = findLocation(_url);
    if (location == nullptr || location->upload_path.empty()) {
        std::cerr << "Upload path not specified in the config for the current location!" << std::endl;
        ServeErrorPage(500);  // Internal Server Error
        return;
    }

    // Convert upload path to absolute path if it's not absolute
    std::string uploadDir = getAbsolutePath(location->upload_path);

    while (std::getline(headerStream, line) && line != "\r") {
        if (line.find("Content-Type: multipart/form-data; boundary=") != std::string::npos) {
            size_t boundaryStart = line.find("boundary=") + 9;
            boundary = line.substr(boundaryStart);
            boundary.erase(boundary.find_last_not_of(" \t\r\n") + 1);
            break;
        }
    }

    // Check if the boundary was found
    if (boundary.empty()) {
        std::cerr << "Boundary not found in headers" << std::endl;
        return;
    }

    std::string boundaryMarker = "--" + boundary;
    std::string endBoundaryMarker = boundaryMarker + "--";
    
    // Position to the start of the multipart data
    size_t startPos = requestBody.find(boundaryMarker);
    size_t endPos = requestBody.rfind(endBoundaryMarker);

    // Validate boundaries
    if (startPos == std::string::npos || endPos == std::string::npos) {
        std::cerr << "Invalid multipart request: boundaries not found!" << std::endl;
        return;
    }

    // Move past the initial boundary marker
    startPos += boundaryMarker.length() + 2;

    // Iterate over each part of the multipart form-data
    while (startPos < endPos) {
        // Find the headers section for the current part
        size_t headerEndPos = requestBody.find("\r\n\r\n", startPos);
        if (headerEndPos == std::string::npos) {
            std::cerr << "Invalid multipart data: header end not found!" << std::endl;
            return;
        }

        // Extract the headers
        std::string partHeaders = requestBody.substr(startPos, headerEndPos - startPos);

        // Extract filename from the headers
        size_t filenamePos = partHeaders.find("filename=\"");
        if (filenamePos == std::string::npos) {
            std::cerr << "Filename not found in headers!" << std::endl;
            return;
        }
        size_t filenameEndPos = partHeaders.find("\"", filenamePos + 10);
        std::string filename = partHeaders.substr(filenamePos + 10, filenameEndPos - (filenamePos + 10));

        // Move past the headers to the content
        startPos = headerEndPos + 4;

        // Find the next boundary marker
        size_t nextBoundary = requestBody.find(boundaryMarker, startPos);
        if (nextBoundary == std::string::npos) {
            std::cerr << "Invalid multipart data: next boundary not found!" << std::endl;
            return;
        }

        // Extract the content between the boundaries (file content)
        std::string fileContent = requestBody.substr(startPos, nextBoundary - startPos - 2);

        // Ensure the uploads directory exists (recursively creates nested directories)
        createDir(uploadDir);

        std::string filePath = uploadDir + "/" + filename;
        std::ofstream outFile(filePath, std::ios::binary);
        if (!outFile) {
            std::cerr << "Error opening output file for writing: " << filePath << std::endl;
            return;
        }
        outFile.write(fileContent.c_str(), fileContent.size());
        outFile.close();

        std::cout << "File uploaded successfully: " << filePath << std::endl;

        // Move to the next part (next boundary marker)
        startPos = nextBoundary + boundaryMarker.length() + 2;
    }

    // Send a success response to the client
    std::string htmlContent = "<html><body><h1>File uploaded successfully!</h1><p>Back to <a href=index.html> Home </a></p></body></html>";
    sendHtmlResponse(htmlContent);
}



// Handling unsupported content types
void Request::handleUnsupportedContentType() {
    std::string htmlContent = "<html><body><h1>Unsupported content type!</h1></body></html>";
    sendHtmlResponse(htmlContent);
}

// Generic function to send HTML response
void Request::sendHtmlResponse(const std::string &htmlContent)
{
    responseHeader(htmlContent, HTTP_200);
    _response += htmlContent;
}

void Request::createDir(const std::string &path) {
    struct stat st;
    std::string currentPath = "";
    std::istringstream pathStream(path);
    std::string directory;

    // If the path is absolute, start building from the root "/"
    if (path[0] == '/') {
        currentPath = "/";
    }

    // Create each directory in the path, handling both absolute and relative paths
    while (std::getline(pathStream, directory, '/')) {
        if (!directory.empty()) {
            currentPath += directory + "/";
            if (stat(currentPath.c_str(), &st) == -1) {
                // Directory doesn't exist, try creating it
                if (mkdir(currentPath.c_str(), 0755) == -1) {  // Use 0755 for directories
                    std::cerr << "Failed to create directory: " << currentPath << std::endl;
                    ServeErrorPage(500);  // Internal Server Error
                    return;
                }
            }
        }
    }
}
