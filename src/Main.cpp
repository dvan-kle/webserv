#include "../include/JsonParser.hpp"
#include "../include/Server.hpp"
#include "../include/Colors.hpp"

#include <fstream>
#include <sstream>
#include <iostream>

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cout << RED << "Usage: " + std::string(argv[0]) + " <config_file>" << RESET << std::endl;
        return 1;
    }

    // open the configuration file
    std::ifstream config_file(argv[1]);
    if (!config_file.is_open()) {
        std::cerr << "Failed to open config file: " << argv[1] << std::endl;
        return 1;
    }

    // read the entire file content into a string
    std::stringstream buffer;
    buffer << config_file.rdbuf();
    std::string config_content = buffer.str();
    config_file.close();

    // initialize the parser with the file content
    JsonParser parser(config_content);
    std::vector<ServerConfig> servers;

    try {
        // parse the configuration file to retrieve server configurations
        servers = parser.parse();
    } catch (const std::exception &e) {
        std::cerr << "Error parsing config: " << e.what() << std::endl;
        return 1;
    }

    // initialize and start the server with the parsed configurations
    Server server(servers);

    return 0;
}
