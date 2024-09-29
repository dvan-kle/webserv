#include "../include/Request.hpp"
#include <fstream>
#include <sstream>

void Request::ServeErrorPage(int error_code) {
    // Check if the error code has a corresponding page in the configuration
    auto it = _config.error_pages.find(error_code);
    if (it != _config.error_pages.end()) {
        std::string error_page_path = it->second;  // Use the exact path as defined in JSON config
        std::ifstream ifstr(error_page_path, std::ios::binary);
        
        // If the custom error page exists, serve it
        if (ifstr) {
            std::string error_content((std::istreambuf_iterator<char>(ifstr)), std::istreambuf_iterator<char>());
            
            _response = _http_version + " " + getStatusMessage(error_code) + "\r\n";
            _response += "Content-Type: text/html\r\n";
            _response += "Content-Length: " + std::to_string(error_content.size()) + "\r\n";
            _response += "Date: " + getCurrentTimeHttpFormat() + "\r\n";
            _response += "Server: " + _config.server_name + "\r\n\r\n";
            _response += error_content;

            // The response is stored in _response and will be sent by the server
            return;
        }
    }

    // If custom error page doesn't exist, serve the fallback page
    std::string fallback_content = R"(
        <!DOCTYPE html>
        <html lang="en">
        <head>
            <meta charset="UTF-8">
            <meta name="viewport" content="width=device-width, initial-scale=1.0">
            <title>Error )" + std::to_string(error_code) + R"(</title>
        </head>
        <body>
            <h1>Error )" + std::to_string(error_code) + R"(</h1>
            <h2>Something went wrong</h2>
            <p>We're sorry, but the page you requested cannot be found or is not accessible.</p>
            <p>Please check the URL or return to the <a href="/">home page</a>.</p>
        </body>
        </html>
    )";

    responseHeader(fallback_content, getStatusMessage(error_code));
    _response += fallback_content;
}
