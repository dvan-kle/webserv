#pragma once

#include <string>
#include <utility>

class HeaderParser {
public:
    static std::pair<std::string, std::string> parseHeaders(const std::string &request);
    static size_t getContentLength(const std::string &headers);
    static std::string getHost(const std::string &headers);
};
