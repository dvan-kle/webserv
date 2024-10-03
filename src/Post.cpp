#include "../include/Request.hpp"
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <map>
#include <regex>

// converts max body size from configuration string (e.g., "1M", "1K") to bytes.
ssize_t Request::convertMaxBodySize(const std::string &input) {
    // regex for valid formats like 1K, 1M, 1G, or plain numbers
    std::regex valid_format("^\\d+(K|M|G)?$");
    if (!std::regex_match(input, valid_format)) {
        // serve 400 Bad Request for invalid format
        std::cerr << "Invalid client_max_body_size format: " << input << std::endl;
        ServeErrorPage(400);
        return -1;
    }

    // extract numeric value and convert units (K/M/G) to bytes
    ssize_t size = std::stoi(input);
    if (input.back() == 'K')
        size *= 1024; // convert Kilobytes to bytes
    else if (input.back() == 'M')
        size *= 1024 * 1024; // convert Megabytes to bytes
    else if (input.back() == 'G')
        size *= 1024 * 1024 * 1024; // convert Gigabytes to bytes

    return size;
}

// handle POST request including body size checks and content type parsing.
void Request::HandlePostRequest(const std::string &requestBody) {
    // convert the max body size from config (e.g., "1M" to 1,048,576 bytes).
    size_t maxBodySize = convertMaxBodySize(_config.client_max_body_size);
    
    // if maxBodySize is invalid (-1), an error page has already been served, so return.
    if (maxBodySize == -1)
        return;

    // Extract Content-Length from headers and check if it exceeds the max allowed body size.
    size_t contentLength = extractContentLength();
    if (contentLength > maxBodySize) {
        // if the request body size is too large, log the error and serve a 413 Payload Too Large error.
        std::cerr << "Request body too large: " << contentLength << " bytes (Max allowed: " << maxBodySize << " bytes)" << std::endl;
        ServeErrorPage(413);
        return;
    }

    // extract Content-Type from headers.
    std::string contentType = extractContentType();
    if (contentType.empty()) {
        // if no Content-Type is provided, serve a 400 Bad Request error.
        ServeErrorPage(400);
        return;
    }

    // route the request body to the appropriate content-type handler based on Content-Type.
    processRequestBody(contentType, requestBody);
}

// extract Content-Length header value from the request headers.
size_t Request::extractContentLength() {
    // default content length is 0 (if not specified).
    size_t contentLength = 0;
    
    // find the position of the "Content-Length:" header in the request headers.
    size_t contentLengthPos = _headers.find("Content-Length:");
    
    // if the "Content-Length:" header is found in the headers.
    if (contentLengthPos != std::string::npos) {
        // find the end of the Content-Length value by looking for the end of the line ("\r\n").
        size_t contentLengthEnd = _headers.find("\r\n", contentLengthPos);
        
        // extract the string value of the Content-Length between the header position and the end of the line.
        std::string contentLengthStr = _headers.substr(contentLengthPos + 15, contentLengthEnd - (contentLengthPos + 15));
        
        // convert the extracted string to an unsigned long long integer (stoull).
        contentLength = std::stoull(contentLengthStr);
    }
    
    // return the extracted content length (or 0 if not present).
    return contentLength;
}

// extract Content-Type header value and normalize it
std::string Request::extractContentType() {
    // create a string stream to iterate through the headers line by line
    std::istringstream headerStream(_headers);
    // variables to store each line and the extracted Content-Type value
    std::string line, contentType;
    
    // loop through each header line until the end of headers ("\r") is found.
    while (std::getline(headerStream, line) && line != "\r") {
        // check if the current line contains the "Content-Type:" header.
        if (line.find("Content-Type:") != std::string::npos) {
            // extract the value after "Content-Type:" by moving 13 positions ahead of the header.
            size_t ctStart = line.find("Content-Type:") + 13;
            // get the substring after "Content-Type:"
            contentType = line.substr(ctStart);
            
            // trim any trailing whitespace characters (spaces, tabs, newlines).
            contentType.erase(contentType.find_last_not_of(" \t\r\n") + 1);

            // exit the loop after finding the Content-Type header.
            break;  
        }
    }

    // normalize content type:
    // 1. convert all characters to lowercase to handle case insensitivity.
    std::transform(contentType.begin(), contentType.end(), contentType.begin(), ::tolower);
    
    // 2. remove any remaining whitespace characters (if any).
    contentType.erase(std::remove_if(contentType.begin(), contentType.end(), ::isspace), contentType.end());

    // return the normalized content type.
    return contentType;
}

// process request body based on the content type
void Request::processRequestBody(const std::string &contentType, const std::string &requestBody) {
    // check if the content type is "application/x-www-form-urlencoded"
    if (contentType == "application/x-www-form-urlencoded") {
        // call the handler for URL-encoded form data
        handleFormUrlEncoded(requestBody);
    } 
    // check if the content type is "multipart/form-data" (commonly used for file uploads)
    else if (contentType.find("multipart/form-data") != std::string::npos) {
        // call the handler for multipart form data (file uploads, etc.)
        handleMultipartFormData(requestBody);
    } 
    // check if the content type is plain text ("text/plain" or "plain/text")
    else if (contentType == "text/plain" || contentType == "plain/text") {
        // call the handler for plain text data
        handlePlainText(requestBody);
    } 
    // check if the content type is JSON ("application/json")
    else if (contentType == "application/json") {
        // call the handler for JSON data
        handleJson(requestBody);
    } 
    // handle unsupported content types
    else {
        // serve a 415 error page (Unsupported Media Type)
        ServeErrorPage(415);
    }
}

// handle x-www-form-urlencoded form data
void Request::handleFormUrlEncoded(const std::string &requestBody) {
    // create an input stream from the request body
    std::istringstream bodyStream(requestBody);
    // variable to hold key-value pairs
    std::string keyValue;
    // map to store parsed form data as key-value pairs
    std::map<std::string, std::string> formData;

    // parse key-value pairs from the body, separated by '&'
    while (std::getline(bodyStream, keyValue, '&')) {
        // find the '=' delimiter between key and value
        size_t pos = keyValue.find('=');
        if (pos != std::string::npos) {
            // extract key and value from the string
            std::string key = keyValue.substr(0, pos);  
            std::string value = keyValue.substr(pos + 1);  
            // store the key-value pair in the map
            formData[key] = value;
        }
    }

    // create an HTML response indicating the form was processed successfully
    std::string htmlContent = "<html><body><h1>Form data received successfully!</h1></body></html>";
    // send the HTML response back to the client
    sendHtmlResponse(htmlContent);
}

// handle plain text data
void Request::handlePlainText(const std::string &requestBody) {
    // create a response string to include the received text
    std::string responseText = "Received plain text: " + requestBody;
    // format the response as an HTML message
    std::string htmlContent = "<html><body><h1>" + responseText + "</h1></body></html>";
    // send the HTML response back to the client
    sendHtmlResponse(htmlContent);
}

// handle JSON data
void Request::handleJson(const std::string &requestBody) {
    // example response message indicating JSON was received (can implement but gotta decide tho)
    std::string htmlContent = "<html><body><h1>Received JSON data!</h1></body></html>";
    // Send the HTML response back to the client
    sendHtmlResponse(htmlContent);
}

// handle multipart form-data (used for file uploads)
void Request::handleMultipartFormData(const std::string &requestBody) {
    // extract boundary from the Content-Type header
    std::string boundary = extractBoundary();
    
    // if no boundary is found, return with an error
    if (boundary.empty()) 
        return;

    // retrieve the upload path from the current location configuration
    auto location = findLocation(_url);
    
    // check if the location is valid and has an upload path configured
    if (location == nullptr || location->upload_path.empty()) {
        std::cerr << "Upload path not specified in config for current location!" << std::endl;
        // Serve 500 Internal Server Error if no upload path is found
        ServeErrorPage(500);  
        return;
    }

    // convert the relative upload path to an absolute path
    std::string uploadDir = getAbsolutePath(location->upload_path);

    // process the multipart form-data content in the request body using the boundary
    processMultipartData(requestBody, boundary, uploadDir);

    // after file uploads are processed successfully, send a success response
    std::string htmlContent = "<html><body><h1>File uploaded successfully!</h1></body></html>";
    
    // send the success HTML response to the client
    sendHtmlResponse(htmlContent);
}

// extract boundary from headers for multipart form-data
std::string Request::extractBoundary() {
    // variable to store the extracted boundary
    std::string boundary;
    // variable to store each line of the headers
    std::string line;
    // stream to read the headers line by line
    std::istringstream headerStream(_headers);

    // loop through each line in the headers
    while (std::getline(headerStream, line) && line != "\r") {
        // check if the line contains the "Content-Type: multipart/form-data" with a boundary
        if (line.find("Content-Type: multipart/form-data; boundary=") != std::string::npos) {
            // locate the start of the boundary value
            size_t boundaryStart = line.find("boundary=") + 9;
            // extract the boundary value from the header line
            boundary = line.substr(boundaryStart);
            // trim any trailing spaces, tabs, or newline characters from the boundary string
            boundary.erase(boundary.find_last_not_of(" \t\r\n") + 1);

            // exit the loop once the boundary is found
            break;
        }
    }

    // if the boundary is empty (not found), log an error and serve a 400 Bad Request
    if (boundary.empty()) {
        std::cerr << "Boundary not found in headers" << std::endl;
        ServeErrorPage(400);
    }

    // return the extracted boundary string
    return boundary;
}

// process each part of multipart/form-data and save uploaded files
void Request::processMultipartData(const std::string &requestBody, const std::string &boundary, const std::string &uploadDir) {
    // create boundary markers to delimit the parts of the multipart data
    std::string boundaryMarker = "--" + boundary; // start boundary marker
    std::string endBoundaryMarker = boundaryMarker + "--"; // end boundary marker (used to indicate the last part)

    // find the position of the start and end boundaries in the request body
    size_t startPos = requestBody.find(boundaryMarker); // position of the first boundary marker
    size_t endPos = requestBody.rfind(endBoundaryMarker); // position of the final boundary marker

    // validate if boundaries are present in the request
    if (startPos == std::string::npos || endPos == std::string::npos) {
        std::cerr << "Invalid multipart request: boundaries not found!" << std::endl;
        return;  // exit if the boundaries are invalid
    }

    // move past the initial boundary marker to the actual content
    startPos += boundaryMarker.length() + 2; // skip the boundary marker and the subsequent CRLF

    // loop to process each part (file or form data) until the end boundary is reached
    while (startPos < endPos) {
        // find the end of the headers for the current part (headers end with an empty line)
        size_t headerEndPos = requestBody.find("\r\n\r\n", startPos);
        if (headerEndPos == std::string::npos)
            return; // exit if headers are incomplete

        // extract the headers for the current part
        std::string partHeaders = requestBody.substr(startPos, headerEndPos - startPos);

        // extract the filename from the headers (if it's a file part)
        std::string filename = extractFilename(partHeaders);

        // move to the file content after the headers
        startPos = headerEndPos + 4;  // skip past the header end marker (CRLF CRLF)

        // find the boundary marker for the next part
        size_t nextBoundary = requestBody.find(boundaryMarker, startPos);
        if (nextBoundary == std::string::npos)
            return;  // exit if the next boundary is missing

        // extract the file content between the boundaries
        std::string fileContent = requestBody.substr(startPos, nextBoundary - startPos - 2); // -2 to remove trailing CRLF

        // save the uploaded file to the specified upload directory
        saveUploadedFile(uploadDir, filename, fileContent);

        // move to the next part by skipping past the boundary marker and its trailing CRLF
        startPos = nextBoundary + boundaryMarker.length() + 2;
    }
}

// extract the filename from multipart headers
std::string Request::extractFilename(const std::string &partHeaders) {
    // find the position of the "filename=" in the headers
    size_t filenamePos = partHeaders.find("filename=\"");
    if (filenamePos == std::string::npos) {
        // return an empty string if no filename is found
        return "";
    }
    
    // find the end position of the filename by locating the closing quote
    size_t filenameEndPos = partHeaders.find("\"", filenamePos + 10);
    
    // extract and return the filename by using substr between the found positions
    return partHeaders.substr(filenamePos + 10, filenameEndPos - (filenamePos + 10));
}

// save the uploaded file to the specified path
void Request::saveUploadedFile(const std::string &uploadDir, const std::string &filename, const std::string &fileContent) {
    // return early if no filename was provided
    if (filename.empty())
        return;

    // ensure the upload directory exists
    createDir(uploadDir); 

    // construct the full file path by appending the filename to the directory
    std::string filePath = uploadDir + "/" + filename;

    // open a file stream to write the uploaded file in binary mode
    std::ofstream outFile(filePath, std::ios::binary);
    if (!outFile) {
        // if fails
        std::cerr << "Error opening file for writing: " << filePath << std::endl;
        return;
    }

    // write the file content to the output file
    outFile.write(fileContent.c_str(), fileContent.size());
    // close the file stream
    outFile.close();

    std::cout << "File uploaded successfully: " << filePath << std::endl;
}

// send HTML response back to the client
void Request::sendHtmlResponse(const std::string &htmlContent) {
    // add response header for HTTP 200 OK status
    responseHeader(htmlContent, HTTP_200);  
    // append the HTML content to the response body
    _response += htmlContent;
}
