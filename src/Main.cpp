#include "../include/Logger.hpp"
#include "../include/ConfigParser.hpp"
#include "../include/Server.hpp"

int main(int argc, char **argv) {
    if (argc < 2) {
        Logger::Error("Usage: " + std::string(argv[0]) + " <config_file>");
        return 1;
    }

    ConfigParser configParser;
    configParser.parseConfig(argv[1]);

    Logger::Success("Configuration parsing completed successfully.");

    const std::vector<ServerConfig>& servers = configParser.getServers();
    Logger::Info("Printing parsed configuration file(s).");

    int server_number = 1;
    for (std::vector<ServerConfig>::const_iterator it = servers.begin(); it != servers.end(); ++it) {
        configParser.printServerConfig(*it, server_number++);
    }

    Logger::Warning("Double check all includes, forbidden stuff, subject goals, eval sheet, etc., before evaluation!");

    Server server(8080);

    return 0;
}
