#include "../include/Request.hpp"

void Request::PostResponse(const std::string &requestBody) {
    std::string contentType;
    std::string line;
    std::istringstream headerStream(_headers);

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

        // Save the file to the "uploads" directory
        std::string uploadDir = "uploads";
        createDir(uploadDir);  // Ensure the uploads directory exists

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
    _response += _http_version + " " + HTTP_200 + CONTYPE_HTML;
    _response += CONTENT_LENGTH + std::to_string(htmlContent.size()) + "\r\n\r\n";
    _response += htmlContent;

    // Send the response back to the client
    ssize_t bytes_written = write(_client_fd, _response.c_str(), _response.size());
    if (bytes_written == -1) {
        std::cerr << "Error: write failed" << std::endl;
    }
}

// if the given directory doesnt exist, it tries to create it
void Request::createDir(const std::string &name) {
    struct stat st = {0};
    if (stat(name.c_str(), &st) == -1) {
        if (mkdir(name.c_str(), 0700) == -1) {
            std::cerr << "Error creating directory: " << name << std::endl;
            // need to handle the error here!!
        }
    }
}
