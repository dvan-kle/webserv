#include "../include/Request.hpp"
#include "../include/JsonParser.hpp"

// Constructor
Request::Request(int client_fd, ServerConfig server) : _client_fd(client_fd), _config(server)
{
    char buffer[1024];
    std::string headers;
    int bytes_read;
    size_t total_bytes_read = 0;

    // Read headers from client
    while (true) {
        bytes_read = read(_client_fd, buffer, sizeof(buffer));
        if (bytes_read == -1) {
            std::cerr << "Error: read failed" << std::endl;
            close(_client_fd);
            exit(EXIT_FAILURE);
        } else if (bytes_read == 0) {
            // Client closed the connection
            break;
        }
        headers.append(buffer, bytes_read);
        total_bytes_read += bytes_read;

        // Check if we've reached the end of headers
        size_t pos = headers.find("\r\n\r\n");
        if (pos != std::string::npos) {
            _headers = headers.substr(0, pos + 4);  // Save headers
            _body = headers.substr(pos + 4);        // Remaining body after headers
            break;
        }
    }

    // Handle transfer encoding or content length
    if (_headers.find("Transfer-Encoding: chunked") != std::string::npos) {
        _body = unchunkRequestBody(_body);  // Handle chunked body
    } else {
        // Handle content-length (for POST data)
        size_t content_length = 0;
        size_t cl_pos = _headers.find("Content-Length:");
        if (cl_pos != std::string::npos) {
            size_t cl_end = _headers.find("\r\n", cl_pos);
            std::string cl_str = _headers.substr(cl_pos + 15, cl_end - (cl_pos + 15));
            content_length = std::stoi(cl_str);
        }

        // Read the rest of the body based on Content-Length
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

// Handle chunked transfer encoding
std::string Request::unchunkRequestBody(const std::string &buffer) {
    std::istringstream stream(buffer);
    std::string line;
    std::string body;

    while (std::getline(stream, line)) {
        int chunkSize;
        std::stringstream hexStream(line);
        hexStream >> std::hex >> chunkSize;
        if (chunkSize == 0) {
            break;  // End of chunked data
        }

        // Read chunk data
        char *chunkData = new char[chunkSize + 1];
        stream.read(chunkData, chunkSize);
        chunkData[chunkSize] = '\0';
        body += std::string(chunkData);
        delete[] chunkData;

        // Skip the CRLF after the chunk
        std::getline(stream, line);
    }
    return body;
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

    // Debugging output
    std::cout << "Matched location path: " << location->path << std::endl;
    std::cout << "Allowed methods: ";
    for (const auto& m : location->methods) {
        std::cout << m << " ";
    }
    std::cout << std::endl;
    std::cout << "Redirection URL: " << location->redirection << std::endl;

        // Check if the location has a redirection
        if (!location->redirection.empty()) {
            sendRedirectResponse(location->redirection, location->return_code);
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

void Request::sendRedirectResponse(const std::string& redirection_url, int return_code) {
    // Map return codes to status messages
    std::string status_line;
    switch (return_code) {
        case 301:
            status_line = "301 Moved Permanently";
            break;
        case 302:
            status_line = "302 Found";
            break;
        case 307:
            status_line = "307 Temporary Redirect";
            break;
        case 308:
            status_line = "308 Permanent Redirect";
            break;
        default:
            // Default to 302 Found if return_code is unrecognized
            status_line = std::to_string(return_code) + " Redirect";
            break;
    }

    // Build the HTTP response
    _response = _http_version + " " + status_line + "\r\n";
    _response += "Location: " + redirection_url + "\r\n";
    _response += "Content-Length: 0\r\n";
    _response += "Server: " + _config.server_name + "\r\n";
    _response += "\r\n";

    // Send the response back to the client
    ssize_t bytes_written = write(_client_fd, _response.c_str(), _response.size());
    if (bytes_written == -1) {
        std::cerr << "Error: write failed" << std::endl;
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


void Request::GetResponse()
{
    std::string filePath = WWW_FOLD + _url;

    // Check if it's a directory
    struct stat pathStat;
    stat(filePath.c_str(), &pathStat);

    if (S_ISDIR(pathStat.st_mode)) {
        // Directory requested; check for autoindex
        auto location = findLocation(_url);
        if (location != nullptr) {
            // Check if an index file is specified and exists in the directory
            std::string indexFilePath = filePath + "/" + location->index;
            struct stat indexStat;
            if (stat(indexFilePath.c_str(), &indexStat) == 0 && S_ISREG(indexStat.st_mode)) {
                // Index file exists, serve the index file
                filePath = indexFilePath;
            } else if (location->autoindex) {
                // Index file doesn't exist, and autoindex is enabled, generate and serve directory listing
                std::string host = _config.listen_host;  // Get host from server config
                int port = _config.listen_port;          // Get port from server config

                // Generate and serve directory listing with the full URL
                std::string dirListing = generateDirectoryListing(filePath, host, port);
                responseHeader(dirListing, HTTP_200);  // Use HTTP_200 or relevant status code
                ssize_t bytes_written = write(_client_fd, _response.c_str(), _response.size());
                if (bytes_written == -1) {
                    std::cerr << "Error: write failed" << std::endl;
                    close(_client_fd);
                    exit(EXIT_FAILURE);
                }
                return;
            } else {
                // Autoindex is disabled and no index file, serve error
                ServeErrorPage(403);  // Forbidden (no index file and no autoindex)
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

    // Send the response
    ssize_t bytes_written = write(_client_fd, _response.c_str(), _response.size());
    if (bytes_written == -1) {
        std::cerr << "Error: write failed" << std::endl;
        close(_client_fd);
        exit(EXIT_FAILURE);
    }
}

std::string Request::generateDirectoryListing(const std::string& directoryPath, const std::string& host, int port) {
    std::ostringstream html;
    html << "<html><head><title>Directory Listing</title></head><body>";
    html << "<h1>Index of " << _url << "</h1>";
    html << "<ul>";

    DIR* dir = opendir(directoryPath.c_str());
    if (dir == nullptr) {
        return "<html><body>Error: Unable to open directory</body></html>";
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string entryName = entry->d_name;
        if (entryName == ".") {
            continue;  // Skip current directory
        }

        // Construct the full URL with host and port
        std::string entryUrl = "http://" + host + ":" + std::to_string(port) + _url + (entryName == ".." ? "/.." : "/" + entryName);

        // Generate correct full links with hostname and port
        html << "<li><a href=\"" << entryUrl << "\">" << entryName << "</a></li>";
    }

    closedir(dir);
    html << "</ul></body></html>";

    return html.str();
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
