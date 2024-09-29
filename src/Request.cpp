#include "../include/Request.hpp"
#include "../include/JsonParser.hpp"
#include "../include/Redirect.hpp"
#include "../include/AutoIndex.hpp"
#include "../include/HeaderParser.hpp"
#include "../include/BodyParser.hpp"
#include "../include/WriteClient.hpp"

bool hasFileExtension(const std::string& url) {
    // Check if the URL contains a file extension (like .html, .js, .css)
    static const std::regex fileExtensionRegex(R"(\.[a-zA-Z0-9]+$)");
    return std::regex_search(url, fileExtensionRegex);
}

Request::Request(int client_fd, ServerConfig server) : _client_fd(client_fd), _config(server)
{
    // Use the HeaderParser to read headers and the initial body content
    std::pair<std::string, std::string> headersAndBody = HeaderParser::readHeaders(_client_fd);
    _headers = headersAndBody.first;
    _body = headersAndBody.second;

    // Handle transfer encoding or content length
    if (HeaderParser::isChunkedEncoding(_headers)) {
        _body = BodyParser::unchunkRequestBody(_body);  // Handle chunked body
    } else {
        size_t content_length = HeaderParser::getContentLength(_headers);
        char buffer[1024];
        int bytes_read;

        while (_body.size() < content_length) {
            bytes_read = read(_client_fd, buffer, sizeof(buffer));
            if (bytes_read == -1) {
                std::cerr << "Error: read failed" << std::endl;
                close(_client_fd);
                exit(EXIT_FAILURE);
            } else if (bytes_read == 0) {
                break;
            }
            _body.append(buffer, bytes_read);
        }
    }
}


// Destructor
Request::~Request() {
    close(_client_fd);
}

void Request::ParseRequest() {
    std::string::size_type pos = _headers.find("\r\n");

    if (pos != std::string::npos) {
        std::string requestLine = _headers.substr(0, pos);
        ParseLine(requestLine);

        auto location = findLocation(_url);
        if (location == nullptr || !isMethodAllowed(location, _method)) {
            ServeErrorPage(405);  // Method Not Allowed
            return;
        }

        if (!location->redirection.empty()) {
            Redirect::sendRedirectResponse(_client_fd, _http_version, location->redirection, location->return_code, _config.server_name);
            return;
        }

        if (isCgiRequest(_url)) {
            executeCGI(_url, _method, _body);
        } else {
            SendResponse(_body);
        }
    } else {
        std::cerr << "Invalid HTTP request" << std::endl;
        ServeErrorPage(400);  // Bad Request
    }
}

// Parse the first line of the HTTP request
void Request::ParseLine(std::string line) {
    std::string::size_type pos = 0;
    std::string::size_type prev = 0;

    pos = line.find(" ");
    _method = line.substr(prev, pos);
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
    if (_method == "GET" || _method == "HEAD") {
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

    // If it's a HEAD request, omit the body
    if (_method == "HEAD") {
        _response = _response.substr(0, _response.find("\r\n\r\n") + 4);
    }

    // Send the response to the client
    WriteClient::safeWriteToClient(_client_fd, _response);
}







void Request::responseHeader(const std::string &htmlContent, const std::string &status_code)
{
    _response = _http_version + " " + status_code + "\r\n";

    if (_url.find(".css") != std::string::npos) {
        _response += "Content-Type: text/css\r\n";
    } else if (_url.find(".js") != std::string::npos) {
        _response += "Content-Type: application/javascript\r\n";
    } else if (_url.find(".json") != std::string::npos) {
        _response += "Content-Type: application/json\r\n";
    } else {
        _response += "Content-Type: text/html\r\n";
    }

    _response += "Content-Length: " + std::to_string(htmlContent.size()) + "\r\n";
    _response += "Date: " + getCurrentTimeHttpFormat() + "\r\n";
    _response += "Server: " + _config.server_name  + "\r\n\r\n";
    _response += htmlContent;  // Include htmlContent here
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
        return true;  // Allow all methods if none are specified (We need to check if this is even good according subject)
    }
    return std::find(location->methods.begin(), location->methods.end(), method) != location->methods.end();
}

std::string getCurrentTimeHttpFormat()
{
    // Get the current time
    std::time_t now = std::time(nullptr);

    std::tm *gmt_time = std::gmtime(&now);
    std::ostringstream ss;

    // Format the time according to HTTP date standard (RFC 7231)
    // Example: "Tue, 17 Sep 2024 16:04:55 GMT"
    ss << std::put_time(gmt_time, "%a, %d %b %Y %H:%M:%S GMT");

    return ss.str();
}