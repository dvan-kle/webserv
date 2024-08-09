#include "../include/Config.hpp"
#include "../include/Colors.hpp"
#include "../include/Logger.hpp"

 /*                      *\
|==========================|
|---------- TEMP ----------|
|^^^^^^^^^^^^^^^^^^^^^^^^^^|
 \*                      */

// Print the server configuration
void printServerConfig(const ServerConfig& config, int server_number) {
    std::cout << "Server " << server_number << " Configuration:" << std::endl;
    
    std::map<std::string, std::string> serverConfigMap = {
        {"Host", config.host},
        {"Port", std::to_string(config.port)},
        {"Server Name", config.server_name},
        {"Client Max Body Size", config.client_max_body_size},
        {"Root", config.root},
        {"Index", config.index}
    };

    // Print out server-level configurations
    for (std::map<std::string, std::string>::const_iterator it = serverConfigMap.begin(); it != serverConfigMap.end(); ++it) {
        if (!it->second.empty() && it->second != "0") {
            std::cout << it->first << ": " << it->second << std::endl;
        }
    }

    // Print out each location block
    for (std::map<std::string, LocationConfig>::const_iterator loc_it = config.locations.begin(); loc_it != config.locations.end(); ++loc_it) {
        std::cout << "Location: " << loc_it->first << std::endl;

        std::map<std::string, std::string> locationConfigMap = {
            {"Root", loc_it->second.root},
            {"Alias", loc_it->second.alias},
            {"Index", loc_it->second.index},
            {"Return Directive", loc_it->second.return_directive},
            {"Client Body Temp Path", loc_it->second.client_body_temp_path},
            {"Autoindex", loc_it->second.autoindex},
            {"Default File", loc_it->second.default_file},
            {"Upload Path", loc_it->second.upload_path}
        };

        // Print out location-level configurations
        for (std::map<std::string, std::string>::const_iterator it = locationConfigMap.begin(); it != locationConfigMap.end(); ++it) {
            if (!it->second.empty()) {
                std::cout << "  " << it->first << ": " << it->second << std::endl;
            }
        }

        // Print out CGI paths
        if (!loc_it->second.cgi_path.empty()) {
            std::cout << "  CGI Path: ";
            for (std::vector<std::string>::const_iterator path_it = loc_it->second.cgi_path.begin(); path_it != loc_it->second.cgi_path.end(); ++path_it) {
                std::cout << *path_it << " ";
            }
            std::cout << std::endl;
        }

        // Print out CGI extensions
        if (!loc_it->second.cgi_extensions.empty()) {
            std::cout << "  CGI Extensions: ";
            for (std::set<std::string>::const_iterator ext_it = loc_it->second.cgi_extensions.begin(); ext_it != loc_it->second.cgi_extensions.end(); ++ext_it) {
                std::cout << *ext_it << " ";
            }
            std::cout << std::endl;
        }

        // Print out accepted methods
        if (!loc_it->second.accepted_methods.empty()) {
            std::cout << "  Allowed Methods: ";
            for (std::set<std::string>::const_iterator method_it = loc_it->second.accepted_methods.begin(); method_it != loc_it->second.accepted_methods.end(); ++method_it) {
                std::cout << *method_it << " ";
            }
            std::cout << std::endl;
        }
    }

    // Print out error pages
    if (!config.error_pages.empty()) {
        for (std::map<int, std::string>::const_iterator error_it = config.error_pages.begin(); error_it != config.error_pages.end(); ++error_it) {
            std::cout << "Error Page for " << error_it->first << ": " << error_it->second << std::endl;
        }
    }

    std::cout << std::endl;
}

 /*                      *\
|==========================|
|---------- MAIN ----------|
|^^^^^^^^^^^^^^^^^^^^^^^^^^|
 \*                      */

int main(int argc, char **argv) {
    if (argc < 2) {
        Logger::Error("Usage: " + std::string(argv[0]) + " <config_file>");
        exit(1);
    }

    std::vector<ServerConfig> servers = parseConfig(argv[1]);
    Logger::Success("Configuration parsing completed successfully.");

    std::cout << std::endl;

    Logger::Info("Printing parsed configuration file(s).");
    int server_number = 1;
    for (const auto& server : servers) {
        printServerConfig(server, server_number++);
        std::cout << std::endl;
    }

    std::cout << std::endl;

    Logger::Warning("Double check all includes, forbidden stuff, subject goals, eval sheet and etc before evaluation.");

    return 0;
}
