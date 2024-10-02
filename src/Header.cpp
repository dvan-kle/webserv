#include "Header.hpp"
#include "Request.hpp"
#include <algorithm>
#include <cstring>

// parses the headers and body from the HTTP request.
std::pair<std::string, std::string> Header::parseHeaders(const std::string &request)
{
    // find the position where headers end and body begins (marked by "\r\n\r\n")
    size_t pos = request.find("\r\n\r\n");
    if (pos == std::string::npos) {
        // if no headers found, return an empty pair
        return std::make_pair("", "");
    }

    // extract the headers and the body from the request
    std::string headers = request.substr(0, pos);
    std::string body = request.substr(pos + 4);

    // return headers and body
    return std::make_pair(headers, body);
}

size_t Header::getContentLength(const std::string &headers)
{
    // find the "Content-Length:" header
    size_t pos = headers.find("Content-Length:");
    if (pos == std::string::npos) {
        // return 0 if Content-Length is not found
        return 0;
    }

    // move the position after "Content-Length:" to find the value
    pos += strlen("Content-Length:");

    // find the end of the line where the Content-Length value ends
    size_t end = headers.find("\r\n", pos);

    // extract the value and convert it to a size_t
    std::string value = headers.substr(pos, end - pos);
    return std::stoi(value);
}

std::string Header::getHost(const std::string &headers)
{
    // find the "Host:" header
    size_t pos = headers.find("Host:");
    if (pos == std::string::npos) {
        // return an empty string if Host is not found
        return "";
    }

    // move the position after "Host:" to get the host value
    pos += strlen("Host:");

    // find the end of the line where the Host value ends
    size_t end = headers.find("\r\n", pos);

    // extract the Host value
    std::string host = headers.substr(pos, end - pos);
    
    // trim any leading and trailing whitespace from the host value
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
    // start the response with the HTTP version and status code
    _response = _http_version + " " + status_code + "\r\n";

    // determine the Content-Type based on the URL extension, defaulting to text/html
    if (_url.find(".css") != std::string::npos) {
        _response += "Content-Type: text/css\r\n";
    } else if (_url.find(".js") != std::string::npos) {
        _response += "Content-Type: application/javascript\r\n";
    } else if (_url.find(".json") != std::string::npos) {
        _response += "Content-Type: application/json\r\n";
    } else {
        _response += "Content-Type: text/html\r\n";
    }

    // add the Content-Length header to indicate the size of the response body
    _response += "Content-Length: " + std::to_string(content.size()) + "\r\n";
    // add the Date header with the current time in HTTP format
    _response += "Date: " + getCurrentTimeHttpFormat() + "\r\n";
    // add the Server header with the server name from the configuration. uses the matched configs server_name
    _response += "Server: " + _config.server_name + "\r\n\r\n"; 
}
