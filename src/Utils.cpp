#include "Request.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <regex>
#include <limits.h>

std::string Request::getCurrentTimeHttpFormat()
{
    // get the current time
    std::time_t now = std::time(nullptr);

    std::tm *gmt_time = std::gmtime(&now);
    std::ostringstream ss;

    // format the time according to HTTP date standard (RFC 7231)
    ss << std::put_time(gmt_time, "%a, %d %b %Y %H:%M:%S GMT");

    return ss.str();
}

LocationConfig* Request::findLocation(const std::string& url) {
    LocationConfig* best_match = nullptr;
    size_t best_match_length = 0;

    for (auto& location : _config.locations) {
        if (url.find(location.path) == 0) {
            size_t path_length = location.path.length();
            if (path_length > best_match_length) {
                best_match = &location;
                best_match_length = path_length;
            }
        }
    }

    if (!best_match) {
        for (auto& location : _config.locations) {
            if (location.path == "/") {
                best_match = &location;
                break;
            }
        }
    }

    return best_match;
}

// Helper function to get absolute path from a relative or absolute one
std::string Request::getAbsolutePath(const std::string &path) {
    char absPath[PATH_MAX];

    // If the path starts with "/", it's already absolute, return it as is
    if (path[0] == '/') {
        return path;
    }

    // Otherwise, treat it as relative to the current working directory
    if (realpath(".", absPath) == nullptr) {
        std::cerr << "Error resolving current working directory!" << std::endl;
        return path;  // Return the original path if we can't resolve the current directory
    }

    // Return the combined absolute path for relative paths
    return std::string(absPath) + "/" + path;
}
