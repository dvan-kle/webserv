#include "../include/Request.hpp"
#include "../include/JsonParser.hpp"
#include "../include/Redirect.hpp"
#include "../include/AutoIndex.hpp"
#include "../include/HeaderParser.hpp"
#include "../include/BodyParser.hpp"

#include <regex>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <algorithm>

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

Request::Request(ServerConfig server, const std::string &request_data)
    : _config(server), _request(request_data)
{
    // No socket I/O in the constructor
}

Request::~Request() {
    // Destructor logic if needed
}

void Request::ParseRequest() {
    // Use HeaderParser to parse headers and body
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

    // Log the normalization process
    std::cerr << "Original URL: " << original_url << " | Normalized URL: " << _url << std::endl;

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

// Implement other methods, ensuring no direct socket I/O
// For example, in GetResponse:

void Request::GetResponse() {
    auto location = findLocation(_url);

    if (location == nullptr) {
        ServeErrorPage(404);  // Return 404 if no matching location is found
        return;
    }

    // Determine if the URL has a file extension
    bool isFile = hasFileExtension(_url);
    std::string filePath;

    if (isFile) {
        // If the URL is a file (e.g., /index.html), append the URL to the root directory
        filePath = location->root + _url;
    } else {
        // Check if the URL exactly matches the location path
        if (_url == location->path) {
            // Serve the index file directly from the root
            filePath = location->root + "/" + location->index;
        } else {
            // If the URL does not have a file extension, treat it as a directory
            std::string dirPath = location->root + _url;
            struct stat pathStat;

            if (stat(dirPath.c_str(), &pathStat) == -1) {
                // Path does not exist
                ServeErrorPage(404);
                return;
            }

            if (S_ISDIR(pathStat.st_mode)) {
                // If it's a directory, append index file
                if (dirPath.back() != '/')
                    dirPath += '/';
                dirPath += location->index;
                filePath = dirPath;
            } else {
                // Path exists but is not a directory
                ServeErrorPage(404);
                return;
            }
        }
    }

    // Now, check if the file exists and is a regular file
    struct stat fileStat;
    if (stat(filePath.c_str(), &fileStat) == -1 || !S_ISREG(fileStat.st_mode)) {
        if (!isFile && location->autoindex) {
            // If autoindex is enabled and the directory exists, generate directory listing
            std::string directoryPath = location->root + _url;
            struct stat dirStat;
            if (stat(directoryPath.c_str(), &dirStat) == 0 && S_ISDIR(dirStat.st_mode)) {
                std::string directoryListing = AutoIndex::generateDirectoryListing(directoryPath, _url, _config.listen_host, _config.listen_port);
                sendHtmlResponse(directoryListing);
                return;
            }
        }
        // File does not exist
        ServeErrorPage(404);
        return;
    }

    // Serve the requested file
    std::ifstream ifstr(filePath, std::ios::binary);
    if (!ifstr) {
        ServeErrorPage(404);  // File Not Found
        return;
    }

    std::string content((std::istreambuf_iterator<char>(ifstr)), std::istreambuf_iterator<char>());

    // Construct the response headers and body
    responseHeader(content, HTTP_200);

    _response += content;
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
    _response += "Server: " + _config.server_name  + "\r\n\r\n";
    // Note: Do not append the content here; it is appended separately
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
    case 500:
        return HTTP_500;
    default:
        return HTTP_200;
    }
}

LocationConfig* Request::findLocation(const std::string& url) {
    LocationConfig* best_match = nullptr;
    size_t best_match_length = 0;

    // Iterate through the available locations in the server configuration
    for (auto& location : _config.locations) {
        if (url.find(location.path) == 0) {
            size_t path_length = location.path.length();
            if (path_length > best_match_length) {
                best_match = &location;
                best_match_length = path_length;
            }
        }
    }

    // If no specific location matches, default to "/"
    if (!best_match) {
        for (auto& location : _config.locations) {
            if (location.path == "/") {
                best_match = &location;
                break;
            }
        }
    }

    // Check if the matched location has CGI capabilities
    if (best_match && !best_match->cgi_extension.empty() && !best_match->cgi_path.empty()) {
        // Ensure the location has a root defined to serve CGI
        if (best_match->root.empty()) {
            std::cerr << "CGI request failed: No valid root defined for " << best_match->path << std::endl;
            return nullptr;
        }
    }

    return best_match;
}

bool Request::isMethodAllowed(LocationConfig* location, const std::string& method) {
    if (location->methods.empty()) {
        return true;  // Allow all methods if none are specified
    }
    return std::find(location->methods.begin(), location->methods.end(), method) != location->methods.end();
}

void Request::sendRedirectResponse(const std::string &redirection_url, int return_code) {
    // Construct the redirect response based on the return code
    _response = _http_version + " " + std::to_string(return_code) + " Redirect\r\n";
    _response += "Location: " + redirection_url + "\r\n";
    _response += "Content-Length: 0\r\n";
    _response += "Connection: close\r\n\r\n";
    _response_ready = true;
}


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

// Implement other methods as needed, ensuring no direct socket I/O
