#include "../include/Libaries.hpp"
#include "../include/ServerConfig.hpp"

// Getters
const std::string& ServerConfig::getHost() const { return host; }
int ServerConfig::getPort() const { return port; }
const std::string& ServerConfig::getServerName() const { return server_name; }
const std::string& ServerConfig::getClientMaxBodySize() const { return client_max_body_size; }
const std::string& ServerConfig::getRoot() const { return root; }
const std::string& ServerConfig::getIndex() const { return index; }

const std::map<std::string, LocationConfig>& ServerConfig::getLocations() const { return locations; }
std::map<std::string, LocationConfig>& ServerConfig::getLocations() { return locations; }

const std::map<int, std::string>& ServerConfig::getErrorPages() const { return error_pages; }
LocationConfig* ServerConfig::getLocation(const std::string& path) {
    auto it = locations.find(path);
    if (it != locations.end())
		return &(it->second);
    return nullptr;
}

// Setters
void ServerConfig::setHost(const std::string& value) { host = value; }
void ServerConfig::setPort(int value) { port = value; }
void ServerConfig::setServerName(const std::string& value) { server_name = value; }
void ServerConfig::setClientMaxBodySize(const std::string& value) { client_max_body_size = value; }
void ServerConfig::setRoot(const std::string& value) { root = value; }
void ServerConfig::setIndex(const std::string& value) { index = value; }
void ServerConfig::addLocation(const std::string& path, const LocationConfig& location) { locations[path] = location; }
void ServerConfig::addErrorPage(int error_code, const std::string& path) { error_pages[error_code] = path; }
