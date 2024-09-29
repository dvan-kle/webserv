#pragma once

#include <string>
#include <utility>

class HeaderParser
{
public:
    static std::pair<std::string, std::string> parseHeaders(const std::string& request_data);
    static size_t getContentLength(const std::string& headers);
    static bool isChunkedEncoding(const std::string& headers);
};
