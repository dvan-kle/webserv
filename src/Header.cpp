#include "Header.hpp"
#include "Request.hpp"
#include <algorithm>
#include <cstring>

std::pair<std::string, std::string> Header::parseHeaders(const std::string &request)
{
    size_t pos = request.find("\r\n\r\n");
    if (pos == std::string::npos) {
        return std::make_pair("", "");
    }
    std::string headers = request.substr(0, pos);
    std::string body = request.substr(pos + 4);
    return std::make_pair(headers, body);
}

size_t Header::getContentLength(const std::string &headers)
{
    size_t pos = headers.find("Content-Length:");
    if (pos == std::string::npos) {
        return 0;
    }
    pos += strlen("Content-Length:");
    size_t end = headers.find("\r\n", pos);
    std::string value = headers.substr(pos, end - pos);
    return std::stoi(value);
}

std::string Header::getHost(const std::string &headers)
{
    size_t pos = headers.find("Host:");
    if (pos == std::string::npos) {
        return "";
    }
    pos += strlen("Host:");
    size_t end = headers.find("\r\n", pos);
    std::string host = headers.substr(pos, end - pos);
    // Trim whitespace
    host.erase(host.begin(), std::find_if(host.begin(), host.end(), [](int ch) {
        return !std::isspace(ch);
    }));
    host.erase(std::find_if(host.rbegin(), host.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), host.end());
    return host;
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
