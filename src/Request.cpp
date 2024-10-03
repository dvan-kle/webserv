#include "Request.hpp"
#include "Header.hpp"

#include <iomanip>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

Request::Request(const std::vector<ServerConfig> &configs, const std::string &request_data, int port): _configs(configs), _request(request_data), _port(port) {}

Request::~Request() {}

// parse the incoming HTTP request
void Request::ParseRequest() {
    // step 1: parse the headers and body
    ParseHeadersAndBody();

    // step 2: ensure headers are valid
    if (_headers.empty()) {
        // return error if headers are missing
        ServeErrorPage(400);
        return;
    }

    // step 3: parse the request line (GET /path HTTP/1.1)
    std::string requestLine = ExtractRequestLine();
    ParseLine(requestLine);

    // step 4: select the appropriate server configuration based on the Host header
    std::string host_header = Header::getHost(_headers);
    ServerConfig* selected_config = selectServerConfig(host_header);
    if (selected_config == nullptr) {
        // return error if no config is found
        ServeErrorPage(500);
        return;
    }
    // save the selected configuration
    _config = *selected_config;

    // step 5: handle redirection, location finding, and request handling
    HandleRequest();
}

void Request::ParseHeadersAndBody() {
    // split the request into headers and body
    std::pair<std::string, std::string> headersAndBody = Header::parseHeaders(_request);
    // store the headers
    _headers = headersAndBody.first;
    // store the body
    _body = headersAndBody.second;
}

std::string Request::ExtractRequestLine() {
    // extract the first line (the request line) from the headers
    size_t line_end_pos = _headers.find("\r\n");
    if (line_end_pos != std::string::npos) {
        // return the request line
        return _headers.substr(0, line_end_pos);
    }
    return std::string();
}

// parse the request line to extract the method, URL, and HTTP version
void Request::ParseLine(const std::string &line) {
    size_t pos = 0;
    size_t prev = 0;

    // extract the HTTP method
    pos = line.find(" ");
    if (pos == std::string::npos) {
        throw std::runtime_error("Error: Invalid HTTP request line (method)");
    }
    // save the method
    _method = line.substr(prev, pos - prev);
    prev = pos + 1;

    // extract the URL
    pos = line.find(" ", prev);
    if (pos == std::string::npos) {
        throw std::runtime_error("Error: Invalid HTTP request line (URL)");
    }
    // save the original URL
    std::string original_url = line.substr(prev, pos - prev);
    prev = pos + 1;

    // extract the HTTP version
    _http_version = line.substr(prev);

    // validate HTTP version
    if (_http_version.empty() || (_http_version != "HTTP/1.1" && _http_version != "HTTP/1.0")) {
        // default to HTTP/1.1 if missing or invalid
        _http_version = "HTTP/1.1";
    }

    // check if the original URL has a trailing slash (and is not just "/")
    bool had_trailing_slash = (original_url.length() > 1 && original_url.back() == '/');

    // normalize the URL by removing trailing slashes
    _url = original_url;
    NormalizeURL();

    // if the URL had a trailing slash and was normalized, set a flag for redirect
    if (had_trailing_slash && _url != original_url) {
        // set flag for redirect if URL was normalized
        _needs_redirect = true; 
    }
}

// normalize the URL by removing trailing slashes, unless it is "/"
void Request::NormalizeURL() {
    // if URL is exactly "/", do not modify
    if (_url == "/") {
        return;
    }

    // remove all trailing slashes
    size_t last = _url.find_last_not_of('/');
    if (last != std::string::npos) {
        // remove trailing slashes
        _url = _url.substr(0, last + 1);
    } else {
        // URL was all slashes, set to "/"
        _url = "/";
    }
}

// select the correct server configuration based on the Host header
ServerConfig* Request::selectServerConfig(const std::string &host_header) {
    if (_configs.empty()) {
        std::cerr << "No server configurations available" << std::endl;
        return nullptr;
    }

    // match based on server_name (Host header) and port
    for (size_t i = 0; i < _configs.size(); ++i) {
        if (_configs[i].server_name == host_header && _configs[i].listen_port == _port) {
            std::cout << "Matched server_name: " << _configs[i].server_name 
                      << " on port " << _configs[i].listen_port << std::endl;
            // return matching config
            return &const_cast<ServerConfig&>(_configs[i]);
        }
    }

    // fallback: return first config that matches the port
    for (size_t i = 0; i < _configs.size(); ++i) {
        if (_configs[i].listen_port == _port) {
            // return first matching port conf
            return &const_cast<ServerConfig&>(_configs[i]);
        }
    }

    // fallback to the first config
    return &const_cast<ServerConfig&>(_configs[0]);
}

// handle the flow after selecting the server config (redirection, methods, CGI)
void Request::HandleRequest() {
    // if the URL was normalized and needs redirection, send a 301 redirect
    if (_needs_redirect) {
        sendRedirectResponse(_url, 301);
        return;
    }

    // find the location based on the URL and check allowed methods
    auto location = findLocation(_url);
    if (location == nullptr || !isMethodAllowed(location, _method)) {
        // return 405 Method Not Allowed if method is not allowed
        ServeErrorPage(405);
        return;
    }

    // handle redirection if required by the location configuration
    if (!location->redirection.empty()) {
        // handle location redirection
        sendRedirectResponse(location->redirection, location->return_code);
        return;
    }

    // check if the request is for CGI
    if (isCgiRequest(_url)) {
        // execute CGI if applicable
        executeCGI(_url, _method, _body);
    } else {
        // handle the different HTTP methods (GET, POST, DELETE)
        if (_method == "GET") {
            HandleGetRequest();
        } else if (_method == "POST") {
            HandlePostRequest(_body);
        } else if (_method == "DELETE") {
            HandleDeleteRequest();
        } else {
            // return 405 Method Not Allowed for unsupported methods
            ServeErrorPage(405);
        }
    }
}

// check if the HTTP method is allowed for the location
bool Request::isMethodAllowed(LocationConfig* location, const std::string& method) {
    if (location->methods.empty())
        // allow all methods if none are specified
        return true;
    for (std::vector<std::string>::iterator it = location->methods.begin(); it != location->methods.end(); ++it) {
        if (*it == method) {
            // method is allowed
            return true;
        }
    }
    // method is not allowed
    return false;
}
