#include "../include/HeaderParser.hpp"
#include <algorithm>
#include <cctype>

std::pair<std::string, std::string> HeaderParser::parseHeaders(const std::string& request_data) {
    size_t pos = request_data.find("\r\n\r\n");
    if (pos != std::string::npos) {
        std::string headers = request_data.substr(0, pos + 4);
        std::string body = request_data.substr(pos + 4);
        return std::make_pair(headers, body);
    } else {
        // Headers are incomplete or missing
        return std::make_pair("", "");
    }
}

size_t HeaderParser::getContentLength(const std::string& headers) {
    size_t cl_pos = headers.find("Content-Length:");
    if (cl_pos != std::string::npos) {
        size_t cl_end = headers.find("\r\n", cl_pos);
        std::string cl_str = headers.substr(cl_pos + 15, cl_end - (cl_pos + 15));

        // Trim whitespace
        cl_str.erase(std::remove_if(cl_str.begin(), cl_str.end(), ::isspace), cl_str.end());

        return static_cast<size_t>(std::stoul(cl_str));
    }
    return 0;
}

bool HeaderParser::isChunkedEncoding(const std::string& headers) {
    return headers.find("Transfer-Encoding: chunked") != std::string::npos;
}
