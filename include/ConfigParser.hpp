#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include "Libaries.hpp"
#include "ServerConfig.hpp"

class ConfigParser {
    private:
        std::vector<ServerConfig> servers;

    public:
		// Getters
        const std::vector<ServerConfig>& getServers() const;

		// Funcs
        void parseConfig(const std::string& configFile);
        void printServerConfig(const ServerConfig& config, int server_number) const;
};

#endif
