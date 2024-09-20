#pragma once

#include "Libaries.hpp"

class HeaderParser {
public:
    static std::pair<std::string, std::string> readHeaders(int client_fd);
    static size_t getContentLength(const std::string& headers);
    static bool isChunkedEncoding(const std::string& headers);
};
