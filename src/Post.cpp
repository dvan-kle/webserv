#include "../include/Request.hpp"

void Request::PostResponse(const std::string &requestBody) {
    // hold the boundary extracted from the HTTP headers
    std::string boundary;
    // hold each line of the headers as they are read one by one
    std::string line;
    // creates an input string stream from the _headers string. _headers contains the raw HTTP headers received from the client. This stream allows easy line-by-line reading of these headers, which is used later to find specific fields (like the boundary)
    std::istringstream headerStream(_headers);

    // extract boundary from headers
    while (std::getline(headerStream, line) && line != "\r") {
        if (line.find("Content-Type: multipart/form-data; boundary=") != std::string::npos) {
            size_t boundaryStart = line.find("boundary=") + 9;
            boundary = line.substr(boundaryStart);
            // Remove any trailing whitespace or line breaks
            boundary.erase(boundary.find_last_not_of(" \t\r\n") + 1);
            // std::cout << "Extracted boundary: " << boundary << std::endl;
            break;
        }
    }

    // check if boundary found
    if (boundary.empty()) {
        std::cerr << "Boundary not found in the request headers" << std::endl;
        return;
    }

    // boundary markers, so we can identify start and end of boundary
    std::string boundaryMarker = "--" + boundary;
    std::string endBoundaryMarker = boundaryMarker + "--";

    // content of the request
    std::string body = requestBody;

    // finds start and end of the multipart data
    size_t startPos = body.find(boundaryMarker);
    size_t endPos = body.rfind(endBoundaryMarker);

    // if no boundaries are found, return an error
    if (startPos == std::string::npos || endPos == std::string::npos) {
        std::cerr << "Invalid multipart request: boundaries not found!" << std::endl;
        return;
    }

    // moves startPos to skip the boundary marker and the following line breaks (\r\n) to get to the actual content
    startPos += boundaryMarker.length() + 2;

    // process each part of the multipart data
    while (startPos < endPos) {
        // looks for the end of the headers by searching for \r\n\r\n (which separates headers from the actual content)
        size_t headerEndPos = body.find("\r\n\r\n", startPos);
        if (headerEndPos == std::string::npos) {
            std::cerr << "Invalid multipart data: header end not found!" << std::endl;
            return;
        }

        // headers is extracted from the body, starting at startPos and ending at headerEndPos. These are typically metadata about the part, such as content type and file name.
        std::string headers = body.substr(startPos, headerEndPos - startPos);
        // std::cout << "Headers: " << headers << std::endl;


        // looks for the filename="..." part in the headers
        size_t filenamePos = headers.find("filename=\"");
        // filename is not found, an error is returned
        if (filenamePos == std::string::npos) {
            std::cerr << "Filename not found in headers!" << std::endl;
            return;
        }
        // if found, the filename is extracted from the headers for later use in saving the file
        size_t filenameEndPos = headers.find("\"", filenamePos + 10);
        std::string filename = headers.substr(filenamePos + 10, filenameEndPos - (filenamePos + 10));

        // moves the startPos past the end of the headers (\r\n\r\n) to the start of the actual file content
        startPos = headerEndPos + 4;

        // this finds the next boundary marker in the body, which indicates the end of the current file data.
        size_t nextBoundary = body.find(boundaryMarker, startPos);
        // if the next boundary is not found, return an error
        if (nextBoundary == std::string::npos) {
            std::cerr << "Invalid multipart data: next boundary not found!" << std::endl;
            return;
        }

        // extracts the file content between the start position and the next boundary, skipping the last \r\n at the end of the file data
        std::string fileContent = body.substr(startPos, nextBoundary - startPos - 2);

        // if uplaods dir doesnt exist, it creates it
        std::string uploadDir = "uploads";
        createDir(uploadDir);

        // constructs the full file path as uploads/filename.
        std::string filePath = uploadDir + "/" + filename;
        // opens a binary file stream to write the file content to the path
        std::ofstream outFile(filePath, std::ios::binary);
        // if file cannot be opened, return an error
        if (!outFile) {
            std::cerr << "Error opening output file for writing: " << filePath << std::endl;
            return;
        }
        // writes the file content and closes the file after successfully writing
        outFile.write(fileContent.c_str(), fileContent.size());
        outFile.close();

        // log success
        // std::cout << "File uploaded successfully: " << filePath << std::endl;

        // moves startPos to the next boundary marker (skipping the current boundary and any trailing \r\n), to process the next file part in the loop
        startPos = nextBoundary + boundaryMarker.length() + 2;
    }

    // constructs an HTTP response indicating success. But we can also maybe make the pages in .html file and then write that instead of in here
    std::string htmlContent = "<html><body><h1>File uploaded successfully!</h1></body></html>";
    _response += _http_version + " " + HTTP_200 + CONTYPE_HTML;
    _response += CONTENT_LENGTH + std::to_string(htmlContent.size()) + "\r\n\r\n";
    _response += htmlContent;

    // sends the response back to the client by writing it to the clients socket
    ssize_t bytes_written = write(_client_fd, _response.c_str(), _response.size());
    if (bytes_written == -1)
        std::cerr << "Error: write failed" << std::endl;
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
