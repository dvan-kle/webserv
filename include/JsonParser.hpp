#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>

// Configuration structure for a server's location block
struct LocationConfig {
    std::string path;
    std::vector<std::string> methods;
    std::string redirection;
    int return_code = 0;
    std::string root;
    bool autoindex = true;
    std::string upload_path;
    std::vector<std::string> cgi_extension;
    std::vector<std::string> cgi_path;
    std::string index;
};

// Configuration structure for a server block
struct ServerConfig {
    std::string listen_host = "0.0.0.0";
    int listen_port = 8080;
    std::string server_name;
    std::unordered_map<int, std::string> error_pages;
    std::string client_max_body_size = "1M";
    std::vector<LocationConfig> locations;
};

// JsonParser class to parse server configurations from input
class JsonParser {
    private:
        std::string input_;
        size_t pos_;

        // Helper methods to extract values from the input
        std::string getNextString();
        int getNextInt();
        bool getNextBool();
        std::vector<std::string> getNextStringArray();
        void expect(char expected);
        void skipWhitespace();
        
        // Methods to parse specific sections of the configuration
        ServerConfig parseServerConfig();
        LocationConfig parseLocationConfig();
        std::unordered_map<int, std::string> parseErrorPages();
        
    public:
        JsonParser(const std::string& input) : input_(input), pos_(0) {}
        std::vector<ServerConfig> parse();
};
