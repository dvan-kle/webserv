#include "../include/JsonParser.hpp"
#include "../include/Server.hpp"

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cout << "Usage: " + std::string(argv[0]) + " <config_file>" << std::endl;
        return 1;
    }

    std::vector<ServerConfig> servers = parseConfig(argc, argv);

    for (const auto& server : servers) {
        std::cout << server.listen_port;
        Server host_server(server.listen_port);
    }

    return 0;
}
