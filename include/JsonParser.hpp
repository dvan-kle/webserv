#pragma once

#include "Libaries.hpp"

enum class JsonType { Null, Bool, Number, String, Array, Object };

struct LocationConfig {
    std::string path;
    std::vector<std::string> methods;
    std::string redirection;
    int return_code = 0;
    std::string root;
    bool autoindex = false;
    std::string index;
    std::string upload_path;
    std::vector<std::string> cgi_extension;  // Now a list of extensions
    std::vector<std::string> cgi_path;       // Now a list of paths
};


struct ServerConfig {
    std::string listen_host = "0.0.0.0";
    int listen_port = 80;
    std::string server_name;
    std::unordered_map<int, std::string> error_pages;
    std::string client_max_body_size;
    std::vector<LocationConfig> locations;
};

struct JsonValue
{
    JsonType type;
    bool bool_value;
    double number_value;
    std::string string_value;
    std::vector<JsonValue> array_value;
    std::unordered_map<std::string, JsonValue> object_value;

    JsonValue() : type(JsonType::Null) {}
};

class JsonParser {
    private:
        std::string input_;
        size_t pos_;

        void jp_skipWhitespace();
        char jp_peek() const;
        char jp_get();
        JsonValue jp_parseValue();
        JsonValue jp_parseNull();
        JsonValue jp_parseBool();
        JsonValue jp_parseNumber();
        JsonValue jp_parseString();
        JsonValue jp_parseArray();
        JsonValue jp_parseObject();
        void jp_expect(const std::string& expected);

    public:
        JsonParser(const std::string& input) : input_(input), pos_(0) {}
        JsonValue jp_parse();
};

std::vector<ServerConfig> parseConfig(int argc, char* argv[]);
std::vector<int> ParsePorts(std::vector<ServerConfig> servers);
