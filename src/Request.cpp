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

// Parse the first line of the HTTP request
void Request::ParseLine(const std::string &line) {
    size_t pos = 0;
    size_t prev = 0;

    pos = line.find(" ");
    _method = line.substr(prev, pos - prev);
    prev = pos + 1;

    pos = line.find(" ", prev);
    _url = line.substr(prev, pos - prev);
    prev = pos + 1;

    _http_version = line.substr(prev);

    // Validate HTTP version
    if (_http_version.empty() || (_http_version != "HTTP/1.1" && _http_version != "HTTP/1.0")) {
        _http_version = "HTTP/1.1";  // Default to HTTP/1.1 if missing or invalid
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

    // Handle CGI requests in GET method
    if (isCgiRequest(_url)) {
        executeCGI(_url, _method, "");  // Empty body for GET requests
        return;
    }

    // Check if the URL maps to a file or directory and construct the file path
    std::string filePath = location->root;
    bool isDirectory = false;

    if (hasFileExtension(_url)) {
        // If the URL is a file (e.g., upload.html), append the URL to the root directory
        filePath += _url;
    } else {
        // If the URL maps to a directory (e.g., /upload)
        filePath += "/" + location->index; // default to index if available
        struct stat pathStat;
        if (stat(filePath.c_str(), &pathStat) == -1 || !S_ISREG(pathStat.st_mode)) {
            if (location->autoindex) {
                isDirectory = true;  // Directory without index
                filePath = location->root + _url;  // Use directory path directly
            } else {
                ServeErrorPage(404);  // File not found
                return;
            }
        }
    }

    // If autoindex is enabled and it's a directory, generate the directory listing
    if (isDirectory && location->autoindex) {
        std::string directoryListing = AutoIndex::generateDirectoryListing(filePath, _url, _config.listen_host, _config.listen_port);
        sendHtmlResponse(directoryListing);
        return;
    }

    // Check if the file exists
    struct stat pathStat;
    if (stat(filePath.c_str(), &pathStat) == -1 || !S_ISREG(pathStat.st_mode)) {
        ServeErrorPage(404);  // File not found
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
    _response = Redirect::generateRedirectResponse(_http_version, redirection_url, return_code, _config.server_name);
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
