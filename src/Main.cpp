#include "../include/JsonParser.hpp"
#include "../include/Server.hpp"

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cout << "Usage: " + std::string(argv[0]) + " <config_file>" << std::endl;
        return 1;
    }

    std::vector<ServerConfig> servers = parseConfig(argc, argv);

    std::vector<int> ports = ParsePorts(servers);
    Server server(ports, servers);

    return 0;
}
