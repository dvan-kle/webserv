#include "Request.hpp"
#include "Header.hpp"
#include "Redirect.hpp"
#include "AutoIndex.hpp"
#include "BodyParser.hpp"
#include <iomanip>
#include <regex>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

Request::Request(const std::vector<ServerConfig> &configs, const std::string &request_data, int port): _configs(configs), _request(request_data), _port(port) {}

Request::~Request() {}

void Request::ParseRequest() {
    // Parse the headers and body
    std::pair<std::string, std::string> headersAndBody = Header::parseHeaders(_request);
    _headers = headersAndBody.first;
    _body = headersAndBody.second;

    if (_headers.empty()) {
        ServeErrorPage(400);
        return;
    }

    size_t line_end_pos = _headers.find("\r\n");
    if (line_end_pos != std::string::npos) {
        std::string requestLine = _headers.substr(0, line_end_pos);
        ParseLine(requestLine);

        // Extract the Host header
        std::string host_header = Header::getHost(_headers);

        // Select the appropriate server config
        ServerConfig* selected_config = selectServerConfig(host_header);
        if (selected_config == nullptr) {
            ServeErrorPage(500);
            return;
        }

        _config = *selected_config;

        // After selecting _config, you can proceed as before
        if (_needs_redirect) {
            sendRedirectResponse(_url, 301);
            return;
        }

        auto location = findLocation(_url);
        if (location == nullptr || !isMethodAllowed(location, _method)) {
            ServeErrorPage(405);
            return;
        }

        if (!location->redirection.empty()) {
            sendRedirectResponse(location->redirection, location->return_code);
            return;
        }

        if (isCgiRequest(_url)) {
            executeCGI(_url, _method, _body);
        } else {
            SendResponse(_body);
        }
    } else {
        ServeErrorPage(400);
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

void Request::SendResponse(const std::string &requestBody) {
    if (_method == "GET") {
        GetResponse();
    } else if (_method == "POST") {
        if (isCgiRequest(_url)) {
            executeCGI(_url, _method, requestBody);
        } else {
            PostResponse(requestBody);
        }
    } else if (_method == "DELETE") {
        DeleteResponse();
    } else {
        ServeErrorPage(405);
    }
}

void Request::GetResponse() {
    auto location = findLocation(_url);

    if (location == nullptr) {
        ServeErrorPage(404);
        return;
    }

    bool isFile = hasFileExtension(_url);
    std::string filePath = location->root;

    // Only append _url if it's not the root path ("/")
    if (_url != location->path) {
        filePath += _url;
    }

    // Check if it's a directory
    struct stat pathStat;
    if (stat(filePath.c_str(), &pathStat) == -1) {
        ServeErrorPage(404);
        return;
    }

    if (S_ISDIR(pathStat.st_mode)) {
        // Directory found, try serving the index file if specified
        if (!location->index.empty()) {
            if (filePath.back() != '/') filePath += '/';  // Ensure trailing slash
            filePath += location->index;  // Append the index file

            // Check if the index file exists and is a regular file
            struct stat fileStat;
            if (stat(filePath.c_str(), &fileStat) == -1 || !S_ISREG(fileStat.st_mode)) {
                // If no index file exists, fall back to autoindex if enabled
                if (location->autoindex) {
                    std::string directoryListing = AutoIndex::generateDirectoryListing(location->root, _url, _config.listen_host, _config.listen_port);
                    sendHtmlResponse(directoryListing);
                    return;
                } else {
                    ServeErrorPage(404);
                    return;
                }
            } else {
                // Serve the index file directly, avoiding recursive request handling
                std::ifstream ifstr(filePath, std::ios::binary);
                if (!ifstr) {
                    ServeErrorPage(404);
                    return;
                }

                std::string content((std::istreambuf_iterator<char>(ifstr)), std::istreambuf_iterator<char>());
                responseHeader(content, HTTP_200);
                _response += content;
                return;  // Prevent further request handling
            }
        } else {
            // No index file specified, handle with autoindex
            if (location->autoindex) {
                std::string directoryListing = AutoIndex::generateDirectoryListing(location->root, _url, _config.listen_host, _config.listen_port);
                sendHtmlResponse(directoryListing);
                return;
            } else {
                ServeErrorPage(404);
                return;
            }
        }
    } else {
        // If it's a file, serve the file directly
        std::ifstream ifstr(filePath, std::ios::binary);
        if (!ifstr) {
            ServeErrorPage(404);
            return;
        }

        std::string content((std::istreambuf_iterator<char>(ifstr)), std::istreambuf_iterator<char>());
        responseHeader(content, HTTP_200);
        _response += content;
    }
}

void Request::responseHeader(const std::string &content, const std::string &status_code)
{
    _response = _http_version + " " + status_code + "\r\n";

    // Determine Content-Type based on URL or default to text/html
    if (_url.find(".css") != std::string::npos) {
        _response += "Content-Type: text/css\r\n";
    } else if (_url.find(".js") != std::string::npos) {
        _response += "Content-Type: application/javascript\r\n";
    } else if (_url.find(".json") != std::string::npos) {
        _response += "Content-Type: application/json\r\n";
    } else {
        _response += "Content-Type: text/html\r\n";
    }

    _response += "Content-Length: " + std::to_string(content.size()) + "\r\n";
    _response += "Date: " + getCurrentTimeHttpFormat() + "\r\n";
    
    // Set the correct server name in the response header
    _response += "Server: " + _config.server_name + "\r\n\r\n";  // Use the matched config's server_name
}
