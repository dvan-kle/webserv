#include "HeaderParser.hpp"
#include <algorithm>
#include <cstring>

std::pair<std::string, std::string> HeaderParser::parseHeaders(const std::string &request)
{
    size_t pos = request.find("\r\n\r\n");
    if (pos == std::string::npos) {
        return std::make_pair("", "");
    }
    std::string headers = request.substr(0, pos);
    std::string body = request.substr(pos + 4);
    return std::make_pair(headers, body);
}

size_t HeaderParser::getContentLength(const std::string &headers)
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

std::string HeaderParser::getHost(const std::string &headers)
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
