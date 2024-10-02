#include "Request.hpp"
#include "Header.hpp"

#include <iomanip>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>



/* ------------------------- *\
|-----------Request-----------|
\* ------------------------- */

Request::Request(const std::vector<ServerConfig> &configs, const std::string &request_data, int port): _configs(configs), _request(request_data), _port(port) {}

Request::~Request() {}



/* ------------------------------ *\
|-----------ParseRequest-----------|
\* ------------------------------ */

void Request::ParseRequest() {
    // Step 1: Parse the headers and body
    ParseHeadersAndBody();

    // Step 2: Ensure headers are valid
    if (_headers.empty()) {
        ServeErrorPage(400);
        return;
    }

    // Step 3: Parse the request line
    std::string requestLine = ExtractRequestLine();
    ParseLine(requestLine);

    // Step 4: Select the appropriate server config
    std::string host_header = Header::getHost(_headers);
    ServerConfig* selected_config = selectServerConfig(host_header);
    if (selected_config == nullptr) {
        ServeErrorPage(500);
        return;
    }
    _config = *selected_config;

    // Step 5: Handle redirection, location finding, and request handling
    HandleRequest();
}

// Split headers and body into two parts
void Request::ParseHeadersAndBody() {
    std::pair<std::string, std::string> headersAndBody = Header::parseHeaders(_request);
    _headers = headersAndBody.first;
    _body = headersAndBody.second;
}

// Extract the first line (the request line) from headers
std::string Request::ExtractRequestLine() {
    size_t line_end_pos = _headers.find("\r\n");
    if (line_end_pos != std::string::npos) {
        return _headers.substr(0, line_end_pos);
    }
    return std::string();
}



/* --------------------------- *\
|-----------ParseLine-----------|
\* --------------------------- */

void Request::ParseLine(const std::string &line) {
    size_t pos = 0;
    size_t prev = 0;

    // Extract the HTTP method
    pos = line.find(" ");
    if (pos == std::string::npos) {
        throw std::runtime_error("Error: Invalid HTTP request line (method)");
    }
    _method = line.substr(prev, pos - prev);
    prev = pos + 1;

    // Extract the URL
    pos = line.find(" ", prev);
    if (pos == std::string::npos) {
        throw std::runtime_error("Error: Invalid HTTP request line (URL)");
    }
    std::string original_url = line.substr(prev, pos - prev);
    prev = pos + 1;

    // Extract the HTTP version
    _http_version = line.substr(prev);

    // Validate HTTP version
    if (_http_version.empty() || (_http_version != "HTTP/1.1" && _http_version != "HTTP/1.0")) {
        _http_version = "HTTP/1.1";  // Default to HTTP/1.1 if missing or invalid
    }

    // Check if the original URL has a trailing slash (and is not just "/")
    bool had_trailing_slash = (original_url.length() > 1 && original_url.back() == '/');

    // Normalize the URL by removing trailing slashes
    _url = original_url;
    NormalizeURL();

    // If the URL had a trailing slash and was normalized, set a flag for redirect
    if (had_trailing_slash && _url != original_url) {
        _needs_redirect = true;
    }
}

void Request::NormalizeURL() {
    // If URL is exactly "/", do not modify
    if (_url == "/") {
        return;
    }

    // Remove all trailing slashes
    size_t last = _url.find_last_not_of('/');
    if (last != std::string::npos) {
        _url = _url.substr(0, last + 1);
    } else {
        // URL was all slashes, set to "/"
        _url = "/";
    }
}



/* ------------------------------------ *\
|-----------selectServerConfig-----------|
\* ------------------------------------ */

ServerConfig* Request::selectServerConfig(const std::string &host_header) {
    if (_configs.empty()) {
        std::cerr << "No server configurations available" << std::endl;
        return nullptr;
    }


    // Match based on server_name (Host header) and port
    for (size_t i = 0; i < _configs.size(); ++i) {
        if (_configs[i].server_name == host_header && _configs[i].listen_port == _port) {
            std::cout << "Matched server_name: " << _configs[i].server_name 
                      << " on port " << _configs[i].listen_port << std::endl;
            return &const_cast<ServerConfig&>(_configs[i]);
        }
    }

    // Fallback: return first config that matches the port
    for (size_t i = 0; i < _configs.size(); ++i) {
        if (_configs[i].listen_port == _port) {
            return &const_cast<ServerConfig&>(_configs[i]);
        }
    }

    return &const_cast<ServerConfig&>(_configs[0]);  // Fallback to the first config
}



/* ------------------------------- *\
|-----------HandleRequest-----------|
\* ------------------------------- */

// Handle the flow after selecting the server config (redirection, methods, CGI)
void Request::HandleRequest() {
    if (_needs_redirect) {
        sendRedirectResponse(_url, 301);
        return;
    }

    // Find the location based on the URL and check allowed methods
    auto location = findLocation(_url);
    if (location == nullptr || !isMethodAllowed(location, _method)) {
        ServeErrorPage(405);
        return;
    }

    // Handle redirection if required by the location configuration
    if (!location->redirection.empty()) {
        sendRedirectResponse(location->redirection, location->return_code);
        return;
    }

    // Check if the request is for CGI
    if (isCgiRequest(_url)) {
        executeCGI(_url, _method, _body);
    } else {
        if (_method == "GET") {
            HandleGetRequest();
        } else if (_method == "POST") {
            HandlePostRequest(_body);
        } else if (_method == "DELETE") {
            HandleDeleteRequest();
        } else {
            ServeErrorPage(405);
        }
    }
}

bool Request::isMethodAllowed(LocationConfig* location, const std::string& method) {
    if (location->methods.empty())
        return true;  // Allow all methods if none are specified
    for (std::vector<std::string>::iterator it = location->methods.begin(); it != location->methods.end(); ++it) {
        if (*it == method) {
            return true;
        }
    }
    return false;
}
