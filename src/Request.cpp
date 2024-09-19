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

        // Check if the method is allowed
        auto location = findLocation(_url);
        if (location == nullptr || !isMethodAllowed(location, _method)) {
            ServeErrorPage(405);  // Method Not Allowed
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
    if (_method == "GET") {
        GetResponse();
    } else if (_method == "POST") {
        PostResponse(requestBody);
    } else if (_method == "DELETE") {
        DeleteResponse();
    } else {
        ServeErrorPage(404);
    }
}


void Request::GetResponse()
{
	if (_url == "/")
		_url = "/index.html";
	std::string responsefile  = WWW_FOLD + _url;
	// std::cout << responsefile << std::endl;
	
	std::ifstream ifstr(responsefile, std::ios::binary);
	if (!ifstr)
		return (PageNotFound());
	std::string htmlContent((std::istreambuf_iterator<char>(ifstr)), std::istreambuf_iterator<char>());
	_response += _http_version + " " + HTTP_200;
	if (_url.find(".css") != std::string::npos)
		_response += CON_TYPE_CSS;
	else
		_response += CONTYPE_HTML;
	_response += CONTENT_LENGTH + std::to_string(htmlContent.size()) + "\r\n";
    _response += "Date: " + getCurrentTimeHttpFormat() + "\r\n";
    _response += "Server: " + _config.server_name  + "\r\n\r\n";
	_response += htmlContent;
	

	ssize_t bytes_written = write(_client_fd, _response.c_str(), strlen(_response.c_str()));
	if (bytes_written == -1)
	{
		std::cerr << "Error: write failed" << std::endl;
		close(_client_fd);
		exit(EXIT_FAILURE);
	}
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