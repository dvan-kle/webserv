#include "../include/Config.hpp"

 /*                         *\
|=============================|
|---------- HELPERS ----------|
|^^^^^^^^^^^^^^^^^^^^^^^^^^^^^|
 \*                         */

// Split a string by a delimiter
std::vector<std::string> split(const std::string &s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter))
        tokens.push_back(token);
    return tokens;
}

// Split a string by space
std::vector<std::string> split_by_space(const std::string &s) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (tokenStream >> token)
        tokens.push_back(token);
    return tokens;
}

// Trim whitespace from both ends of a string
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos)
        return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

 /*                              *\
|==================================|
|---------- ServerConfig ----------|
|^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^|
 \*                              */

// Parse the configuration file and return a vector of ServerConfig objects
std::vector<ServerConfig> parseConfig(const std::string& configFile) {
    std::vector<ServerConfig> servers;
    ServerConfig config;
    std::ifstream file(configFile);
    std::string line;
    std::string current_location;

    while (std::getline(file, line)) {
        // Remove leading and trailing whitespace and handle comments
        line = trim(line);
        line = line.substr(0, line.find('#'));
        line = trim(line);

        // Skip empty lines
        if (line.empty())
            continue;

        std::istringstream iss(line);
        std::string key;
        iss >> key;

        if (key == "server") {
            if (!config.host.empty()) {
                servers.push_back(config); // Save previous server config
                config = ServerConfig(); // Reset for the new server block
            }
            continue;
        } else if (key == "}") {
            // End of current block
            current_location = "";
        } else if (key == "host") {
            iss >> config.host;
        } else if (key == "listen") {
            iss >> config.port;
        } else if (key == "server_name") {
            iss >> config.server_name;
        } else if (key == "client_max_body_size") {
            iss >> config.client_max_body_size;
        } else if (key == "root") {
            if (current_location.empty())
                iss >> config.root;
            else
                iss >> config.locations[current_location].root;
        } else if (key == "index") {
            if (current_location.empty())
                iss >> config.index;
            else
                iss >> config.locations[current_location].index;
        } else if (key == "location") {
            iss >> current_location;
            config.locations[current_location] = LocationConfig();
            config.locations[current_location].path = current_location;
        } else if (key == "alias") {
            iss >> config.locations[current_location].alias;
        } else if (key == "return") {
            std::string status_code;
            std::string redirect_path;
            iss >> status_code;
            std::getline(iss, redirect_path);
            redirect_path = trim(redirect_path);
            config.locations[current_location].return_directive = status_code + " " + redirect_path;
        } else if (key == "client_body_temp_path") {
            iss >> config.locations[current_location].client_body_temp_path;
        } else if (key == "autoindex") {
            iss >> config.locations[current_location].autoindex;
        } else if (key == "allow_methods") {
            std::string method;
            while (iss >> method)
                config.locations[current_location].accepted_methods.insert(method);
        } else if (key == "default_file") {
            iss >> config.locations[current_location].default_file;
        } else if (key == "cgi_path") {
            std::string path;
            while (iss >> path)
                config.locations[current_location].cgi_path.push_back(path);
        } else if (key == "cgi_ext") {
            std::string ext;
            while (iss >> ext) {
                config.locations[current_location].cgi_extensions.insert(ext);
            }
        } else if (key == "upload_path") {
            iss >> config.locations[current_location].upload_path;
        } else if (key == "error_page") {
            int error_code;
            std::string error_path;
            iss >> error_code >> error_path;
            config.error_pages[error_code] = error_path;
        }
    }
    if (!config.host.empty())
        servers.push_back(config); // Save the last server config

    return servers;
}
