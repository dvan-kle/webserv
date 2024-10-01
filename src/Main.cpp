#include "../include/JsonParser.hpp"
#include "../include/Server.hpp"
#include "../include/Colors.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cout << RED <<  "Usage: " + std::string(argv[0]) + " <config_file>" << RESET << std::endl;
        return 1;
    }

    // Step 1: Read the configuration file into a string.
    std::ifstream config_file(argv[1]);
    if (!config_file.is_open()) {
        std::cerr << "Failed to open config file: " << argv[1] << std::endl;
        return 1;
    }

    std::stringstream buffer;
    buffer << config_file.rdbuf();
    std::string config_content = buffer.str();
    config_file.close();

    // Step 2: Instantiate the parser with the configuration content.
    JsonParser parser(config_content);
    std::vector<ServerConfig> servers;

    // Step 3: Try parsing the configuration.
    try {
        servers = parser.parse();
    } catch (const std::exception &e) {
        std::cerr << "Error parsing config: " << e.what() << std::endl;
        return 1;
    }

    // Step 4: Initialize the server.
    Server server(servers);

    return 0;
}
