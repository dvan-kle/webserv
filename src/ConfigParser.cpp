#include "../include/Libaries.hpp"
#include "../include/ConfigParser.hpp"

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

void ConfigParser::parseConfig(const std::string& configFile) {
    ServerConfig config;
    std::ifstream file(configFile);
    std::string line;
    std::string current_location;

    while (std::getline(file, line)) {
        line = trim(line);
        line = line.substr(0, line.find('#'));
        line = trim(line);

        if (line.empty())
            continue;

        std::istringstream iss(line);
        std::string key;
        iss >> key;

        if (key == "server") {
            if (!config.getHost().empty()) {
                servers.push_back(config);
                config = ServerConfig();
            }
            continue;
        } else if (key == "}") {
            current_location = "";
        } else if (key == "host") {
            std::string host_value;
            iss >> host_value;
            config.setHost(host_value);
        } else if (key == "listen") {
            int port_value;
            iss >> port_value;
            config.setPort(port_value);
        } else if (key == "server_name") {
            std::string server_name_value;
            iss >> server_name_value;
            config.setServerName(server_name_value);
        } else if (key == "client_max_body_size") {
            std::string client_max_body_size_value;
            iss >> client_max_body_size_value;
            config.setClientMaxBodySize(client_max_body_size_value);
        } else if (key == "root") {
            std::string root_value;
            iss >> root_value;
            if (current_location.empty())
                config.setRoot(root_value);
            else
                config.getLocations().at(current_location).setRoot(root_value);
        } else if (key == "index") {
            std::string index_value;
            iss >> index_value;
            if (current_location.empty())
                config.setIndex(index_value);
            else
                config.getLocations().at(current_location).setIndex(index_value);
        } else if (key == "location") {
            iss >> current_location;
            LocationConfig locConfig;
            locConfig.setPath(current_location);
            config.addLocation(current_location, locConfig);
        } else if (key == "alias") {
            std::string alias_value;
            iss >> alias_value;
            config.getLocations().at(current_location).setAlias(alias_value);
        } else if (key == "return") {
            std::string status_code;
            std::string redirect_path;
            iss >> status_code;
            std::getline(iss, redirect_path);
            redirect_path = trim(redirect_path);
            config.getLocations().at(current_location).setReturnDirective(status_code + " " + redirect_path);
        } else if (key == "client_body_temp_path") {
            std::string client_body_temp_path_value;
            iss >> client_body_temp_path_value;
            config.getLocations().at(current_location).setClientBodyTempPath(client_body_temp_path_value);
        } else if (key == "autoindex") {
            std::string autoindex_value;
            iss >> autoindex_value;
            config.getLocations().at(current_location).setAutoindex(autoindex_value);
        } else if (key == "allow_methods") {
            std::string method;
            while (iss >> method)
                config.getLocations().at(current_location).addAcceptedMethod(method);
        } else if (key == "default_file") {
            std::string default_file_value;
            iss >> default_file_value;
            config.getLocations().at(current_location).setDefaultFile(default_file_value);
        } else if (key == "cgi_path") {
            std::string path_value;
            while (iss >> path_value)
                config.getLocations().at(current_location).addCgiPath(path_value);
        } else if (key == "cgi_ext") {
            std::string ext_value;
            while (iss >> ext_value)
                config.getLocations().at(current_location).addCgiExtension(ext_value);
        } else if (key == "upload_path") {
            std::string upload_path_value;
            iss >> upload_path_value;
            config.getLocations().at(current_location).setUploadPath(upload_path_value);
        } else if (key == "error_page") {
            int error_code;
            std::string error_path;
            iss >> error_code >> error_path;
            config.addErrorPage(error_code, error_path);
        }
    }

    if (!config.getHost().empty()) {
        servers.push_back(config);
    }
}

const std::vector<ServerConfig>& ConfigParser::getServers() const {
    return servers;
}

void ConfigParser::printServerConfig(const ServerConfig& config, int server_number) const {
    std::cout << "Server " << server_number << " Configuration:" << std::endl;

    // Using getters to access ServerConfig data
    std::map<std::string, std::string> serverConfigMap = {
        {"Host", config.getHost()},
        {"Port", std::to_string(config.getPort())},
        {"Server Name", config.getServerName()},
        {"Client Max Body Size", config.getClientMaxBodySize()},
        {"Root", config.getRoot()},
        {"Index", config.getIndex()}
    };

    // Print out server-level configurations
    for (std::map<std::string, std::string>::const_iterator it = serverConfigMap.begin(); it != serverConfigMap.end(); ++it) {
        if (!it->second.empty() && it->second != "0") {
            std::cout << it->first << ": " << it->second << std::endl;
        }
    }

    // Print out each location block
    const std::map<std::string, LocationConfig>& locations = config.getLocations();
    for (std::map<std::string, LocationConfig>::const_iterator loc_it = locations.begin(); loc_it != locations.end(); ++loc_it) {
        std::cout << "Location: " << loc_it->first << std::endl;

        // Using getters to access LocationConfig data
        std::map<std::string, std::string> locationConfigMap = {
            {"Root", loc_it->second.getRoot()},
            {"Alias", loc_it->second.getAlias()},
            {"Index", loc_it->second.getIndex()},
            {"Return Directive", loc_it->second.getReturnDirective()},
            {"Client Body Temp Path", loc_it->second.getClientBodyTempPath()},
            {"Autoindex", loc_it->second.getAutoindex()},
            {"Default File", loc_it->second.getDefaultFile()},
            {"Upload Path", loc_it->second.getUploadPath()}
        };

        // Print out location-level configurations
        for (std::map<std::string, std::string>::const_iterator it = locationConfigMap.begin(); it != locationConfigMap.end(); ++it) {
            if (!it->second.empty()) {
                std::cout << "  " << it->first << ": " << it->second << std::endl;
            }
        }

        // Print out CGI paths
        const std::vector<std::string>& cgiPaths = loc_it->second.getCgiPath();
        if (!cgiPaths.empty()) {
            std::cout << "  CGI Path: ";
            for (std::vector<std::string>::const_iterator path_it = cgiPaths.begin(); path_it != cgiPaths.end(); ++path_it) {
                std::cout << *path_it << " ";
            }
            std::cout << std::endl;
        }

        // Print out CGI extensions
        const std::set<std::string>& cgiExtensions = loc_it->second.getCgiExtensions();
        if (!cgiExtensions.empty()) {
            std::cout << "  CGI Extensions: ";
            for (std::set<std::string>::const_iterator ext_it = cgiExtensions.begin(); ext_it != cgiExtensions.end(); ++ext_it) {
                std::cout << *ext_it << " ";
            }
            std::cout << std::endl;
        }

        // Print out accepted methods
        const std::set<std::string>& acceptedMethods = loc_it->second.getAcceptedMethods();
        if (!acceptedMethods.empty()) {
            std::cout << "  Allowed Methods: ";
            for (std::set<std::string>::const_iterator method_it = acceptedMethods.begin(); method_it != acceptedMethods.end(); ++method_it) {
                std::cout << *method_it << " ";
            }
            std::cout << std::endl;
        }
    }

    // Print out error pages
    const std::map<int, std::string>& errorPages = config.getErrorPages();
    if (!errorPages.empty()) {
        for (std::map<int, std::string>::const_iterator error_it = errorPages.begin(); error_it != errorPages.end(); ++error_it) {
            std::cout << "Error Page for " << error_it->first << ": " << error_it->second << std::endl;
        }
    }

    std::cout << std::endl;
}

