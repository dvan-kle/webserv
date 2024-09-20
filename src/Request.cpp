#include "../include/Request.hpp"
#include "../include/JsonParser.hpp"
#include "../include/Redirect.hpp"
#include "../include/AutoIndex.hpp"
#include "../include/HeaderParser.hpp"
#include "../include/BodyParser.hpp"
#include "../include/WriteClient.hpp"

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
            executeCGI(WWW_FOLD + _url, _method, _body);
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
}

void Request::SendResponse(const std::string &requestBody) {
    if (_method == "GET" || _method == "HEAD") {
        GetResponse();
    } else if (_method == "POST") {
        PostResponse(requestBody);
    } else if (_method == "DELETE") {
        DeleteResponse();
    } else {
        ServeErrorPage(405);  // Method Not Allowed
    }
}


void Request::GetResponse() {
    std::string filePath = WWW_FOLD + _url;
    struct stat pathStat;
    stat(filePath.c_str(), &pathStat);

    if (S_ISDIR(pathStat.st_mode)) {
        auto location = findLocation(_url);
        if (location != nullptr) {
            std::string indexFilePath = filePath + "/" + location->index;
            struct stat indexStat;
            if (stat(indexFilePath.c_str(), &indexStat) == 0 && S_ISREG(indexStat.st_mode)) {
                filePath = indexFilePath;
            } else if (location->autoindex) {
                std::string host = _config.listen_host;
                int port = _config.listen_port;
                std::string dirListing = AutoIndex::generateDirectoryListing(filePath, _url, host, port);
                responseHeader(dirListing, HTTP_200);
                WriteClient::safeWriteToClient(_client_fd, _response);
                return;
            } else {
                ServeErrorPage(403);
                return;
            }
        }
    }

    // Serve the requested file (or index file in the case of a directory)
    std::ifstream ifstr(filePath, std::ios::binary);
    if (!ifstr) {
        ServeErrorPage(404);  // File Not Found
        return;
    }

    std::string htmlContent((std::istreambuf_iterator<char>(ifstr)), std::istreambuf_iterator<char>());

    responseHeader(htmlContent, HTTP_200);

    // If it's a HEAD request, do not include the body
    if (_method == "HEAD") {
        // Remove the body from the response
        _response = _response.substr(0, _response.find("\r\n\r\n") + 4);
    } else {
        // Append the body for GET requests
        _response += htmlContent;
    }

    WriteClient::safeWriteToClient(_client_fd, _response);
}


void Request::responseHeader(std::string htmlContent, const std::string status_code)
{
    _response += _http_version + " " + status_code;
	if (_url.find(".css") != std::string::npos)
		_response += CON_TYPE_CSS;
	else
		_response += CONTYPE_HTML;
	_response += CONTENT_LENGTH + std::to_string(htmlContent.size()) + "\r\n";
    _response += "Date: " + getCurrentTimeHttpFormat() + "\r\n";
    _response += "Server: " + _config.server_name  + "\r\n\r\n";
	_response += htmlContent;
}

std::string Request::getCurrentTimeHttpFormat()
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

LocationConfig* Request::findLocation(const std::string& url) {
    LocationConfig* best_match = nullptr;
    size_t best_match_length = 0;

    for (auto& location : _config.locations) {
        if (url.find(location.path) == 0) {  // Match the path prefix
            size_t path_length = location.path.length();
            if (path_length > best_match_length) {
                best_match = &location;
                best_match_length = path_length;
            }
        }
    }
    return best_match;  // Return the best matching location
}

bool Request::isMethodAllowed(LocationConfig* location, const std::string& method) {
    if (location->methods.empty()) {
        return true;  // Allow all methods if none are specified (We need to check if this is even good according subject)
    }
    return std::find(location->methods.begin(), location->methods.end(), method) != location->methods.end();
}
