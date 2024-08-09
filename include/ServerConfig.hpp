#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include "Libaries.hpp"
#include "LocationConfig.hpp"

class ServerConfig {
    private:
        std::string host;
        int port = 0;
        std::string server_name;
        std::string client_max_body_size;
        std::string root;
        std::string index;
        std::map<std::string, LocationConfig> locations;
        std::map<int, std::string> error_pages;

    public:
        // Getters
        const std::string& getHost() const;
        int getPort() const;
        const std::string& getServerName() const;
        const std::string& getClientMaxBodySize() const;
        const std::string& getRoot() const;
        const std::string& getIndex() const;

        // Const version of getLocations
        const std::map<std::string, LocationConfig>& getLocations() const;

        // Non-const version of getLocations
        std::map<std::string, LocationConfig>& getLocations();

        const std::map<int, std::string>& getErrorPages() const;
        LocationConfig* getLocation(const std::string& path);

        // Setters
        void setHost(const std::string& value);
        void setPort(int value);
        void setServerName(const std::string& value);
        void setClientMaxBodySize(const std::string& value);
        void setRoot(const std::string& value);
        void setIndex(const std::string& value);
        void addLocation(const std::string& path, const LocationConfig& location);
        void addErrorPage(int error_code, const std::string& path);
};

#endif
