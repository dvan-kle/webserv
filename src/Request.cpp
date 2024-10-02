#include "Request.hpp"
#include "HeaderParser.hpp"
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

std::string getCurrentTimeHttpFormat()
{
    // Get the current time
    std::time_t now = std::time(nullptr);

    std::tm *gmt_time = std::gmtime(&now);
    std::ostringstream ss;

    // Format the time according to HTTP date standard (RFC 7231)
    ss << std::put_time(gmt_time, "%a, %d %b %Y %H:%M:%S GMT");

    return ss.str();
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

bool hasFileExtension(const std::string& url) {
    static const std::regex fileExtensionRegex(R"(\.[a-zA-Z0-9]+$)");
    return std::regex_search(url, fileExtensionRegex);
}

Request::Request(const std::vector<ServerConfig> &configs, const std::string &request_data, int port)
    : _configs(configs), _request(request_data), _port(port) // Initialize _configs
{
    // Select the appropriate ServerConfig based on Host header
    // This will be done in ParseRequest
}

Request::~Request() {
    // Destructor logic if needed
}

ServerConfig* Request::selectServerConfig(const std::string &host_header, int port) {
    if (_configs.empty()) {
        std::cerr << "No server configurations available" << std::endl;
        return nullptr;
    }


    // Match based on server_name (Host header) and port
    for (size_t i = 0; i < _configs.size(); ++i) {
        if (_configs[i].server_name == host_header && _configs[i].listen_port == port) {
            std::cout << "Matched server_name: " << _configs[i].server_name 
                      << " on port " << _configs[i].listen_port << std::endl;
            return &const_cast<ServerConfig&>(_configs[i]);
        }
    }

    // Fallback: return first config that matches the port
    for (size_t i = 0; i < _configs.size(); ++i) {
        if (_configs[i].listen_port == port) {
            return &const_cast<ServerConfig&>(_configs[i]);
        }
    }

    return &const_cast<ServerConfig&>(_configs[0]);  // Fallback to the first config
}






void Request::ParseRequest() {
    // Parse the headers and body
    std::pair<std::string, std::string> headersAndBody = HeaderParser::parseHeaders(_request);
    _headers = headersAndBody.first;
    _body = headersAndBody.second;

    if (_headers.empty()) {
        std::cerr << "Invalid HTTP request: Headers incomplete or missing" << std::endl;
        ServeErrorPage(400);  // Bad Request
        return;
    }

    size_t line_end_pos = _headers.find("\r\n");
    if (line_end_pos != std::string::npos) {
        std::string requestLine = _headers.substr(0, line_end_pos);
        ParseLine(requestLine);

        // Extract the Host header
        std::string host_header = HeaderParser::getHost(_headers);

        // Here, instead of using a default of port 80, you should use the `_port` passed from the server
        int port = _port;  // Use the passed-in port instead of defaulting to 80

        // Select the appropriate server config
        ServerConfig* selected_config = selectServerConfig(host_header, port);
        if (selected_config == nullptr) {
            std::cerr << "No valid server configuration found for host: " << host_header << std::endl;
            ServeErrorPage(500);  // Internal Server Error
            return;
        }

        _config = *selected_config;

        // After selecting _config, you can proceed as before

        if (_needs_redirect) {
            // Send a 301 Moved Permanently redirect response
            _response = _http_version + " 301 Moved Permanently\r\n";
            _response += "Location: " + _url + "\r\n";
            _response += "Content-Length: 0\r\n";
            _response += "Connection: close\r\n\r\n";
            _response_ready = true;
            return;
        }

        auto location = findLocation(_url);
        if (location == nullptr || !isMethodAllowed(location, _method)) {
            ServeErrorPage(405);  // Method Not Allowed
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
        std::cerr << "Invalid HTTP request line" << std::endl;
        ServeErrorPage(400);  // Bad Request
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
        // Check if the request is a CGI request
        if (isCgiRequest(_url)) {
            executeCGI(_url, _method, requestBody);
        } else {
            PostResponse(requestBody);
        }
    } else if (_method == "DELETE") {
        DeleteResponse();
    } else {
        ServeErrorPage(405);  // Method Not Allowed
    }
}

void Request::GetResponse() {
    auto location = findLocation(_url);

    if (location == nullptr) {
        ServeErrorPage(404);  // Return 404 if no matching location is found
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



LocationConfig* Request::findLocation(const std::string& url) {
    LocationConfig* best_match = nullptr;
    size_t best_match_length = 0;

    for (auto& location : _config.locations) {
        if (url.find(location.path) == 0) {
            size_t path_length = location.path.length();
            if (path_length > best_match_length) {
                best_match = &location;
                best_match_length = path_length;
            }
        }
    }

    if (!best_match) {
        for (auto& location : _config.locations) {
            if (location.path == "/") {
                best_match = &location;
                break;
            }
        }
    }

    return best_match;
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


std::string Request::getStatusMessage(int statuscode)
{
    switch (statuscode)
    {
    case 200:
        return HTTP_200;
    case 400:
        return HTTP_400;
    case 403:
        return HTTP_403;
    case 404:
        return HTTP_404;
    case 405:
        return HTTP_405;
    case 413:
        return HTTP_413;
    case 415:
        return HTTP_415;
    case 500:
        return HTTP_500;
    default:
        return HTTP_200;
    }
}

bool Request::isMethodAllowed(LocationConfig* location, const std::string& method) {
    if (location->methods.empty()) {
        return true;  // Allow all methods if none are specified
    }
    return std::find(location->methods.begin(), location->methods.end(), method) != location->methods.end();
}

void Request::sendRedirectResponse(const std::string &redirection_url, int return_code) {
    std::string status_line;

    // Set appropriate status message based on return code
    switch (return_code) {
        case 301: status_line = "301 Moved Permanently";
            break;
        case 302: status_line = "302 Found";
            break;
        case 307: status_line = "307 Temporary Redirect";
            break;
        case 308: status_line = "308 Permanent Redirect";
            break;
        default: status_line = std::to_string(return_code) + " Redirect";
            break;
    }

    // Build the HTTP response
    std::ostringstream response;
    response << _http_version << " " << status_line << "\r\n";
    response << "Location: " << redirection_url << "\r\n";
    response << "Content-Type: text/html\r\n";
    response << "Content-Length: 0\r\n";
    response << "Date: " << getCurrentTimeHttpFormat() << "\r\n";
    response << "Server: " << _config.server_name << "\r\n\r\n";

    _response = response.str();
    _response_ready = true;
}


// Implement other methods as needed, ensuring no direct socket I/O
