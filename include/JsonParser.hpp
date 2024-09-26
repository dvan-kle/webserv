#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>

// Server Configuration Structure
struct LocationConfig {
    std::string path;
    std::vector<std::string> methods;
    std::string redirection;
    int return_code = 0;
    std::string root;
    bool autoindex = false;
    std::string upload_path;
    std::vector<std::string> cgi_extension;
    std::vector<std::string> cgi_path;
    std::string index;
};

struct ServerConfig {
    std::string listen_host = "0.0.0.0";
    int listen_port = 80;
    std::string server_name;
    std::unordered_map<int, std::string> error_pages;
    std::string client_max_body_size;
    std::vector<LocationConfig> locations;
};

// Simple JsonParser that hardcodes the fields and types expected in the input.
class JsonParser {
    private:
        std::string input_;
        size_t pos_;

        // Utility methods for extracting values
        std::string getNextString();
        int getNextInt();
        bool getNextBool();
        std::vector<std::string> getNextStringArray();
        void expect(char expected);
        void skipWhitespace();
        
        // Parsing methods for specific fields
        ServerConfig parseServerConfig();
        LocationConfig parseLocationConfig();
        std::unordered_map<int, std::string> parseErrorPages();
        
    public:
        JsonParser(const std::string& input) : input_(input), pos_(0) {}
        std::vector<ServerConfig> parse();
};
