#include "../include/Request.hpp"

#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>

std::string getStatusMessage(int statuscode)
{
    // Returns the appropriate HTTP status message based on the status code.
    switch (statuscode)
    {
    case 200:
        return HTTP_200;
    case 400:
        return HTTP_400;
    case 403:
        return HTTP_403;
    case 404:
        return HTTP_404;
    case 405:
        return HTTP_405;
    case 413:
        return HTTP_413;
    case 415:
        return HTTP_415;
    case 500:
        return HTTP_500;
    default:
        return HTTP_200;
    }
}

void Request::ServeErrorPage(int error_code) {
    // check if the error code has a custom error page in the server configuration
    auto it = _config.error_pages.find(error_code);

    // if a custom error page is found for the error code
    if (it != _config.error_pages.end()) {
        // get the path of the custom error page
        std::string error_page_path = it->second;
        // open the custom error page file
        std::ifstream ifstr(error_page_path, std::ios::binary);
        
        // if the custom error page exists, serve it
        if (ifstr) {
            // read the content of the file
            std::string error_content((std::istreambuf_iterator<char>(ifstr)), std::istreambuf_iterator<char>());
            
            // build the HTTP response with the custom error page content
            _response = _http_version + " " + getStatusMessage(error_code) + "\r\n";
            _response += "Content-Type: text/html\r\n";
            _response += "Content-Length: " + std::to_string(error_content.size()) + "\r\n";
            _response += "Date: " + getCurrentTimeHttpFormat() + "\r\n";
            _response += "Server: " + _config.server_name + "\r\n\r\n";
            
            // append the error page content to the response
            _response += error_content;

            // the response is stored in _response and is ready to be sent
            return;
        }
    }

    // if no custom error page is found or the file doesn't exist, serve a default fallback page
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

    // add the fallback content to the HTTP response headers
    responseHeader(fallback_content, getStatusMessage(error_code));
    // append the fallback content to the response
    _response += fallback_content;
}
