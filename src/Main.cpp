#include "../include/JsonParser.hpp"
#include "../include/Server.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

std::vector<int> ParsePorts(std::vector<ServerConfig> servers) {
    std::vector<int> ports;
    for (const auto& server : servers) {
        ports.push_back(server.listen_port);
    }
    return ports;
}

std::vector<std::string> ParseHosts(std::vector<ServerConfig> servers) {
	std::vector<std::string> hosts;
	for (const auto& server : servers) {
		hosts.push_back(server.listen_host);
	}
	return hosts;
}

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

    // Step 4: Extract ports and initialize the server.
    std::vector<int> ports = ParsePorts(servers);
	std::vector<std::string> hosts = ParseHosts(servers);
    Server server(ports, hosts, servers);

    return 0;
}
