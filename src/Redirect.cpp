#include "../include/Redirect.hpp"
#include "../include/Request.hpp"

#include "../include/Redirect.hpp"
#include <sstream>
#include <ctime>
#include <iomanip>

std::string getCurrentTimeHttpFormat3()
{
    // Get the current time
    std::time_t now = std::time(nullptr);

    std::tm *gmt_time = std::gmtime(&now);
    std::ostringstream ss;

    // Format the time according to HTTP date standard (RFC 7231)
    ss << std::put_time(gmt_time, "%a, %d %b %Y %H:%M:%S GMT");

    return ss.str();
}

std::string Redirect::generateRedirectResponse(const std::string& http_version, const std::string& redirection_url, int return_code, const std::string& server_name) {
    std::string status_line;

    switch (return_code) {
        case 301: status_line = "301 Moved Permanently";
            break;
        case 302: status_line = "302 Found";
            break;
        case 307: status_line = "307 Temporary Redirect";
            break;
        case 308: status_line = "308 Permanent Redirect";
            break;
        default: status_line = std::to_string(return_code) + " Redirect";
            break;
    }

    // Build the HTTP response
    std::ostringstream response;
    response << http_version << " " << status_line << "\r\n";
    response << "Location: " << redirection_url << "\r\n";
    response << "Content-Type: text/html\r\n";
    response << "Content-Length: 0\r\n";
    response << "Date: " << getCurrentTimeHttpFormat3() << "\r\n";
    response << "Server: " << server_name << "\r\n\r\n";

    return response.str();
}
