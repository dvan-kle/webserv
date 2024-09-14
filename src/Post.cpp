#include "../include/Request.hpp"

void Request::PostResponse(const std::string &requestBody) {
    std::string boundary;
    std::string line;
    std::istringstream headerStream(_headers);

    // Extract boundary from headers
    while (std::getline(headerStream, line) && line != "\r") {
        if (line.find("Content-Type: multipart/form-data; boundary=") != std::string::npos) {
            size_t boundaryStart = line.find("boundary=") + 9;
            boundary = line.substr(boundaryStart);
            // Remove any trailing whitespace or line breaks
            boundary.erase(boundary.find_last_not_of(" \t\r\n") + 1);
            std::cout << "Extracted boundary: " << boundary << std::endl;
            break;
        }
    }

    if (boundary.empty()) {
        std::cerr << "Boundary not found in the request headers" << std::endl;
        return;
    }

    std::string boundaryMarker = "--" + boundary;
    std::string endBoundaryMarker = boundaryMarker + "--";

    // Now process the body
    std::string body = requestBody;

    size_t startPos = body.find(boundaryMarker);
    size_t endPos = body.rfind(endBoundaryMarker);

    if (startPos == std::string::npos || endPos == std::string::npos) {
        std::cerr << "Invalid multipart request: boundaries not found!" << std::endl;
        return;
    }

    // Move past the initial boundary and CRLF
    startPos += boundaryMarker.length() + 2;

    // Iterate through each part of the multipart data
    while (startPos < endPos) {
        // Find the header and data separator
        size_t headerEndPos = body.find("\r\n\r\n", startPos);
        if (headerEndPos == std::string::npos) {
            std::cerr << "Invalid multipart data: header end not found!" << std::endl;
            return;
        }

        // Extract headers
        std::string headers = body.substr(startPos, headerEndPos - startPos);
        std::cout << "Headers: " << headers << std::endl;

        // Extract filename from headers
        size_t filenamePos = headers.find("filename=\"");
        if (filenamePos == std::string::npos) {
            std::cerr << "Filename not found in headers!" << std::endl;
            return;
        }
        size_t filenameEndPos = headers.find("\"", filenamePos + 10);
        std::string filename = headers.substr(filenamePos + 10, filenameEndPos - (filenamePos + 10));

        // Move to the file data start
        startPos = headerEndPos + 4;

        // Find the next boundary marker
        size_t nextBoundary = body.find(boundaryMarker, startPos);
        if (nextBoundary == std::string::npos) {
            std::cerr << "Invalid multipart data: next boundary not found!" << std::endl;
            return;
        }

        // Extract file content
        std::string fileContent = body.substr(startPos, nextBoundary - startPos - 2); // Skip trailing CRLF

        // Create uploads directory if it does not exist
        std::string uploadDir = "uploads";
        createDir(uploadDir);

        // Save the file content to the uploads directory
        std::string filePath = uploadDir + "/" + filename;
        std::ofstream outFile(filePath, std::ios::binary);
        if (!outFile) {
            std::cerr << "Error opening output file for writing: " << filePath << std::endl;
            return;
        }
        outFile.write(fileContent.c_str(), fileContent.size());
        outFile.close();

        std::cout << "File uploaded successfully: " << filePath << std::endl;

        // Move to the next part (skip boundary and CRLF)
        startPos = nextBoundary + boundaryMarker.length() + 2;
    }

    // Send response to the client
    std::string htmlContent = "<html><body><h1>File uploaded successfully!</h1></body></html>";
    _response += _http_version + " " + HTTP_200 + CONTYPE_HTML;
    _response += CONTENT_LENGTH + std::to_string(htmlContent.size()) + "\r\n\r\n";
    _response += htmlContent;

    ssize_t bytes_written = write(_client_fd, _response.c_str(), _response.size());
    if (bytes_written == -1)
        std::cerr << "Error: write failed" << std::endl;
}

void Request::createDir(const std::string &name) {
    struct stat st = {0};
    if (stat(name.c_str(), &st) == -1) {
        if (mkdir(name.c_str(), 0700) == -1) {
            std::cerr << "Error creating directory: " << name << std::endl;
            // Handle the error as needed
        }
    }
}
