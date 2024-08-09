#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "Libaries.hpp"

struct LocationConfig {
    std::string path;
    std::string alias;
    std::string index;
    std::string return_directive;
    std::string root;
    std::string client_body_temp_path;
    std::string autoindex;
    std::set<std::string> accepted_methods;
    std::string default_file;
    std::set<std::string> cgi_extensions;
    std::vector<std::string> cgi_path;
    std::string upload_path;
};

struct ServerConfig {
    std::string host;
    int port = 0;
    std::string server_name;
    std::string client_max_body_size;
    std::string root;
    std::string index;
    std::map<std::string, LocationConfig> locations;
    std::map<int, std::string> error_pages;
};

std::vector<std::string> split(const std::string &s, char delimiter);
std::vector<std::string> split_by_space(const std::string &s);
std::string trim(const std::string& str);
std::vector<ServerConfig> parseConfig(const std::string& configFile);
void printServerConfig(const ServerConfig& config, int server_number);

#endif
