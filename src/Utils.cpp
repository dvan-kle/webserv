#include "Request.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <regex>

std::string Request::getCurrentTimeHttpFormat()
{
    // Get the current time
    std::time_t now = std::time(nullptr);

    std::tm *gmt_time = std::gmtime(&now);
    std::ostringstream ss;

    // Format the time according to HTTP date standard (RFC 7231)
    ss << std::put_time(gmt_time, "%a, %d %b %Y %H:%M:%S GMT");

    return ss.str();
}

bool Request::hasFileExtension(const std::string& url) {
    static const std::regex fileExtensionRegex(R"(\.[a-zA-Z0-9]+$)");
    return std::regex_search(url, fileExtensionRegex);
}

bool Request::isMethodAllowed(LocationConfig* location, const std::string& method) {
    if (location->methods.empty())
        return true;  // Allow all methods if none are specified
    return std::find(location->methods.begin(), location->methods.end(), method) != location->methods.end();
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
