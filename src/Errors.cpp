#include "../include/Request.hpp"

void Request::ServeErrorPage(int error_code) {
    // Check if the error code has a corresponding page in the configuration
    auto it = _config.error_pages.find(error_code);
    if (it != _config.error_pages.end()) {
        std::string error_page_path = WWW_FOLD + it->second; // Use the exact path as defined in JSON config
        std::ifstream ifstr(error_page_path, std::ios::binary);
        
        // If the custom error page exists, serve it
        if (ifstr) {
            std::string error_content((std::istreambuf_iterator<char>(ifstr)), std::istreambuf_iterator<char>());
            _response += _http_version + " " + std::to_string(error_code) + " Error\r\n";
            _response += CONTYPE_HTML;
            _response += CONTENT_LENGTH + std::to_string(error_content.size()) + "\r\n";
            _response += "Server: " + _config.server_name + "\r\n\r\n";
            _response += error_content;

            ssize_t bytes_written = write(_client_fd, _response.c_str(), _response.size());
            if (bytes_written == -1) {
                std::cerr << "Error: write failed" << std::endl;
            }
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

    _response += _http_version + " " + std::to_string(error_code) + " Error\r\n";
    _response += CONTYPE_HTML;
    _response += CONTENT_LENGTH + std::to_string(fallback_content.size()) + "\r\n";
    _response += "Server: " + _config.server_name + "\r\n\r\n";
    _response += fallback_content;

    ssize_t bytes_written = write(_client_fd, _response.c_str(), _response.size());
    if (bytes_written == -1) {
        std::cerr << "Error: write failed" << std::endl;
    }
}

LocationConfig* Request::findLocation(const std::string& url) {
    LocationConfig* best_match = nullptr;
    size_t best_match_length = 0;

    for (auto& location : _config.locations) {
        if (url.find(location.path) == 0) {  // Match the path prefix
            size_t path_length = location.path.length();
            if (path_length > best_match_length) {
                best_match = &location;
                best_match_length = path_length;
            }
        }
    }
    return best_match;  // Return the best matching location
}



bool Request::isMethodAllowed(LocationConfig* location, const std::string& method) {
    if (location->methods.empty()) {
        return true;  // Allow all methods if none are specified
    }
    return std::find(location->methods.begin(), location->methods.end(), method) != location->methods.end();
}
