#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <set>
#include <algorithm>

struct LocationConfig {
    std::string path;
    std::string alias;
    std::string index;
    std::string return_directive;
    std::string root;
    std::string client_body_temp_path;
    std::string autoindex;
    std::set<std::string> accepted_methods;
    std::string default_file;
    std::set<std::string> cgi_extensions;
    std::vector<std::string> cgi_path;
    std::string upload_path;
};

struct ServerConfig {
    std::string host;
    int port = 0;
    std::string server_name;
    std::string client_max_body_size;
    std::string root;
    std::string index;
    std::map<std::string, LocationConfig> locations;
    std::map<int, std::string> error_pages;
};

std::vector<std::string> split(const std::string &s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::vector<std::string> split_by_space(const std::string &s) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (tokenStream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

ServerConfig parseConfig(const std::string& configFile) {
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
        if (line.empty()) {
            continue;
        }

        // Remove semicolons at the end of the line
        if (line.back() == ';') {
            line.pop_back();
        }

        std::istringstream iss(line);
        std::string key;
        iss >> key;

        if (key == "server") {
            // Skip '{' after 'server'
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
            if (current_location.empty()) {
                iss >> config.root;
            } else {
                iss >> config.locations[current_location].root;
            }
        } else if (key == "index") {
            if (current_location.empty()) {
                iss >> config.index;
            } else {
                iss >> config.locations[current_location].index;
            }
        } else if (key == "location") {
            iss >> current_location;
            config.locations[current_location] = LocationConfig();
            config.locations[current_location].path = current_location;
        } else if (key == "alias") {
            iss >> config.locations[current_location].alias;
        } else if (key == "return") {
            iss >> config.locations[current_location].return_directive;
        } else if (key == "client_body_temp_path") {
            iss >> config.locations[current_location].client_body_temp_path;
        } else if (key == "autoindex") {
            iss >> config.locations[current_location].autoindex;
        } else if (key == "allow_methods") {
            std::string method;
            while (iss >> method) {
                config.locations[current_location].accepted_methods.insert(method);
            }
        } else if (key == "default_file") {
            iss >> config.locations[current_location].default_file;
        } else if (key == "cgi_path") {
            std::string path;
            while (iss >> path) {
                config.locations[current_location].cgi_path.push_back(path);
            }
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

    return config;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }

    ServerConfig config = parseConfig(argv[1]);
    std::cout << "Server listening on host: " << config.host << "; port: " << config.port << std::endl;
    std::cout << "Server name: " << config.server_name << ";" << std::endl;
    std::cout << "Client max body size: " << config.client_max_body_size << ";" << std::endl;
    std::cout << "Root directory: " << config.root << ";" << std::endl;
    std::cout << "Index file: " << config.index << ";" << std::endl;
    for (const auto& location : config.locations) {
        std::cout << "Location: " << location.second.path << std::endl;
        if (!location.second.root.empty()) std::cout << "  Root: " << location.second.root << ";" << std::endl;
        if (!location.second.alias.empty()) std::cout << "  Alias: " << location.second.alias << ";" << std::endl;
        if (!location.second.index.empty()) std::cout << "  Index: " << location.second.index << ";" << std::endl;
        if (!location.second.return_directive.empty()) std::cout << "  Return: " << location.second.return_directive << ";" << std::endl;
        if (!location.second.client_body_temp_path.empty()) std::cout << "  Client body temp path: " << location.second.client_body_temp_path << ";" << std::endl;
        if (!location.second.autoindex.empty()) std::cout << "  Autoindex: " << location.second.autoindex << ";" << std::endl;
        if (!location.second.default_file.empty()) std::cout << "  Default file: " << location.second.default_file << ";" << std::endl;
        if (!location.second.cgi_path.empty()) {
            std::cout << "  CGI path: ";
            for (const auto& path : location.second.cgi_path) {
                std::cout << path << " ";
            }
            std::cout << ";" << std::endl;
        }
        if (!location.second.cgi_extensions.empty()) {
            std::cout << "  CGI extensions: ";
            for (const auto& ext : location.second.cgi_extensions) {
                std::cout << ext << " ";
            }
            std::cout << ";" << std::endl;
        }
        if (!location.second.upload_path.empty()) std::cout << "  Upload path: " << location.second.upload_path << ";" << std::endl;
        if (!location.second.accepted_methods.empty()) {
            std::cout << "  Allowed methods: ";
            for (const auto& method : location.second.accepted_methods) {
                std::cout << method << " ";
            }
            std::cout << ";" << std::endl;
        }
    }
    for (const auto& error_page : config.error_pages) {
        std::cout << "Error page for " << error_page.first << ": " << error_page.second << ";" << std::endl;
    }
    return 0;
}
