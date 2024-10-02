#include "../include/Redirect.hpp"
#include "../include/Request.hpp"
#include <sstream>
#include <ctime>
#include <iomanip>

void Request::sendRedirectResponse(const std::string &redirection_url, int return_code) {
    std::string status_line;

    // Set appropriate status message based on return code
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
    response << _http_version << " " << status_line << "\r\n";
    response << "Location: " << redirection_url << "\r\n";
    response << "Content-Type: text/html\r\n";
    response << "Content-Length: 0\r\n";
    response << "Date: " << getCurrentTimeHttpFormat() << "\r\n";
    response << "Server: " << _config.server_name << "\r\n\r\n";

    _response = response.str();
    _response_ready = true;
}
