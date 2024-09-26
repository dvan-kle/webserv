#include "../include/JsonParser.hpp"

void JsonParser::jp_skipWhitespace() {
    while (pos_ < input_.length() && std::isspace(input_[pos_])) {
        pos_++;
    }
}

char JsonParser::jp_peek() const {
    if (pos_ < input_.length())
        return input_[pos_];
    return '\0';
}

char JsonParser::jp_get() {
    if (pos_ < input_.length())
        return input_[pos_++];
    return '\0';
}

JsonValue JsonParser::jp_parseValue() {
    jp_skipWhitespace();
    char c = jp_peek();
    if (c == 'n')
        return jp_parseNull();
    if (c == 't' || c == 'f')
        return jp_parseBool();
    if (c == '-' || std::isdigit(c))
        return jp_parseNumber();
    if (c == '"')
        return jp_parseString();
    if (c == '[')
        return jp_parseArray();
    if (c == '{')
        return jp_parseObject();

    throw std::runtime_error(std::string("Unexpected character: ") + c);
}

JsonValue JsonParser::jp_parseNull() {
    jp_expect("null");
    return JsonValue();
}

JsonValue JsonParser::jp_parseBool() {
    if (input_.substr(pos_, 4) == "true") {
        pos_ += 4;
        JsonValue value;
        value.type = JsonType::Bool;
        value.bool_value = true;
        return value;
    } else if (input_.substr(pos_, 5) == "false") {
        pos_ += 5;
        JsonValue value;
        value.type = JsonType::Bool;
        value.bool_value = false;
        return value;
    } else
        throw std::runtime_error("Invalid boolean value");
}

JsonValue JsonParser::jp_parseNumber() {
    size_t start_pos = pos_;
    if (jp_peek() == '-')
        pos_++;
    if (jp_peek() == '0')
        pos_++;
    else if (std::isdigit(jp_peek()))
        while (std::isdigit(jp_peek())) pos_++;
    else
        throw std::runtime_error("Invalid number");
    if (jp_peek() == '.') {
        pos_++;
        if (!std::isdigit(jp_peek()))
            throw std::runtime_error("Invalid number");
        while (std::isdigit(jp_peek()))
            pos_++;
    }
    if (jp_peek() == 'e' || jp_peek() == 'E') {
        pos_++;
        if (jp_peek() == '+' || jp_peek() == '-')
            pos_++;
        if (!std::isdigit(jp_peek()))
            throw std::runtime_error("Invalid number");
        while (std::isdigit(jp_peek()))
            pos_++;
    }
    std::string number_str = input_.substr(start_pos, pos_ - start_pos);
    JsonValue value;
    value.type = JsonType::Number;
    value.number_value = std::stod(number_str);
    return value;
}

JsonValue JsonParser::jp_parseString() {
    jp_expect("\"");
    std::string result;
    while (true) {
        if (pos_ >= input_.length())
            throw std::runtime_error("Unterminated string");
        char c = jp_get();
        if (c == '"')
            break;
        else if (c == '\\') {
            if (pos_ >= input_.length())
                throw std::runtime_error("Unterminated string");
            c = jp_get();
            if (c == '"' || c == '\\' || c == '/')
                 result += c;
            else if (c == 'b')
                result += '\b';
            else if (c == 'f')
                result += '\f';
            else if (c == 'n')
                result += '\n';
            else if (c == 'r')
                result += '\r';
            else if (c == 't')
                result += '\t';
            else if (c == 'u')
                throw std::runtime_error("Unicode escape sequences are not supported");
            else
                throw std::runtime_error(std::string("Invalid escape character: \\") + c);
        } else
            result += c;
    }
    JsonValue value;
    value.type = JsonType::String;
    value.string_value = result;
    return value;
}

JsonValue JsonParser::jp_parseArray() {
    jp_expect("[");
    JsonValue value;
    value.type = JsonType::Array;
    jp_skipWhitespace();
    if (jp_peek() == ']') {
        jp_get();  // Consume ']'
        return value;
    }
    while (true) {
        JsonValue element = jp_parseValue();
        value.array_value.push_back(element);
        jp_skipWhitespace();
        if (jp_peek() == ',') {
            jp_get();  // Consume ','
            jp_skipWhitespace();
        } else if (jp_peek() == ']') {
            jp_get();  // Consume ']'
            break;
        } else
            throw std::runtime_error("Expected ',' or ']'");
    }
    return value;
}

JsonValue JsonParser::jp_parseObject() {
    jp_expect("{");
    JsonValue value;
    value.type = JsonType::Object;
    jp_skipWhitespace();
    if (jp_peek() == '}') {
        jp_get();  // Consume '}'
        return value;
    }
    while (true) {
        jp_skipWhitespace();
        if (jp_peek() != '"')
            throw std::runtime_error("Expected string key");
        JsonValue key = jp_parseString();
        jp_skipWhitespace();
        jp_expect(":");
        JsonValue val = jp_parseValue();
        value.object_value[key.string_value] = val;
        jp_skipWhitespace();
        if (jp_peek() == ',') {
            jp_get();  // Consume ','
            jp_skipWhitespace();
        } else if (jp_peek() == '}') {
            jp_get();  // Consume '}'
            break;
        } else
            throw std::runtime_error("Expected ',' or '}'");
    }
    return value;
}

void JsonParser::jp_expect(const std::string& expected) {
    for (char c : expected) {
        if (jp_get() != c)
            throw std::runtime_error(std::string("Expected '") + expected + "'");
    }
}

JsonValue JsonParser::jp_parse() {
    jp_skipWhitespace();
    JsonValue value = jp_parseValue();
    jp_skipWhitespace();
    if (pos_ != input_.length())
        throw std::runtime_error("Extra characters after JSON data");
    return value;
}

void parseConfigFromJsonValue(const JsonValue& json, std::vector<ServerConfig>& servers) {
    if (json.type != JsonType::Object || json.object_value.find("servers") == json.object_value.end()) {
        throw std::runtime_error("Invalid configuration format: 'servers' key not found");
    }
    const JsonValue& servers_json = json.object_value.at("servers");
    if (servers_json.type != JsonType::Array) {
        throw std::runtime_error("'servers' should be an array");
    }
    for (const JsonValue& server_json : servers_json.array_value) {
        ServerConfig server;
        if (server_json.type != JsonType::Object) {
            throw std::runtime_error("Each server should be an object");
        }
        const auto& server_obj = server_json.object_value;
        // Parse server-level settings
        if (server_obj.find("listen_host") != server_obj.end()) {
            server.listen_host = server_obj.at("listen_host").string_value;
        }
        if (server_obj.find("listen_port") != server_obj.end()) {
            server.listen_port = static_cast<int>(server_obj.at("listen_port").number_value);
        }
        if (server_obj.find("server_name") != server_obj.end()) {
            server.server_name = server_obj.at("server_name").string_value;
        }
        if (server_obj.find("client_max_body_size") != server_obj.end()) {
            server.client_max_body_size = server_obj.at("client_max_body_size").string_value;
        }
        if (server_obj.find("error_pages") != server_obj.end()) {
            const auto& error_pages_json = server_obj.at("error_pages");
            if (error_pages_json.type != JsonType::Object) {
                throw std::runtime_error("'error_pages' should be an object");
            }
            for (const auto& [code_str, uri_json] : error_pages_json.object_value) {
                int code = std::stoi(code_str);
                server.error_pages[code] = uri_json.string_value;
            }
        }
        // Parse locations
        if (server_obj.find("locations") != server_obj.end()) {
            const auto& locations_json = server_obj.at("locations");
            if (locations_json.type != JsonType::Array) {
                throw std::runtime_error("'locations' should be an array");
            }
            for (const JsonValue& loc_json : locations_json.array_value) {
                LocationConfig loc;
                if (loc_json.type != JsonType::Object) {
                    throw std::runtime_error("Each location should be an object");
                }
                const auto& loc_obj = loc_json.object_value;
                if (loc_obj.find("path") != loc_obj.end()) {
                    loc.path = loc_obj.at("path").string_value;
                }
                if (loc_obj.find("methods") != loc_obj.end()) {
                    const auto& methods_json = loc_obj.at("methods");
                    if (methods_json.type != JsonType::Array) {
                        throw std::runtime_error("'methods' should be an array");
                    }
                    for (const JsonValue& method_json : methods_json.array_value) {
                        loc.methods.push_back(method_json.string_value);
                    }
                }
                if (loc_obj.find("redirection") != loc_obj.end()) {
                    loc.redirection = loc_obj.at("redirection").string_value;
                }
                if (loc_obj.find("return_code") != loc_obj.end()) {
                    loc.return_code = static_cast<int>(loc_obj.at("return_code").number_value);
                }
                if (loc_obj.find("root") != loc_obj.end()) {
                    loc.root = loc_obj.at("root").string_value;
                }
                if (loc_obj.find("autoindex") != loc_obj.end()) {
                    loc.autoindex = loc_obj.at("autoindex").bool_value;
                }
                if (loc_obj.find("index") != loc_obj.end()) {
                    loc.index = loc_obj.at("index").string_value;
                }
                if (loc_obj.find("upload_path") != loc_obj.end()) {
                    loc.upload_path = loc_obj.at("upload_path").string_value;
                }
                if (loc_obj.find("cgi_extension") != loc_obj.end()) {
                    loc.cgi_extension = loc_obj.at("cgi_extension").string_value;
                }
                if (loc_obj.find("cgi_path") != loc_obj.end()) {
                    loc.cgi_path = loc_obj.at("cgi_path").string_value;
                }
                server.locations.push_back(loc);
            }
        }
        servers.push_back(server);
    }
}

std::vector<ServerConfig> parseConfig(int argc, char* argv[]) {
    std::string config_file;
    if (argc > 1) {
        config_file = argv[1];
    }

    // Read the configuration file
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cout << "Cannot open configuration file: " << config_file << std::endl;
        exit(1);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string config_content = buffer.str();
    file.close();

    // Parse the JSON configuration
    JsonParser parser(config_content);
    JsonValue json_root;
    try {
        json_root = parser.jp_parse();
    } catch (const std::exception& e) {
        std::cout << "Failed to parse JSON configuration: " << e.what() << std::endl;
        exit(1);
    }

    // Extract server configurations
    std::vector<ServerConfig> servers;
    try {
        parseConfigFromJsonValue(json_root, servers);
    } catch (const std::exception& e) {
        std::cout << "Failed to parse configuration: " << e.what() << std::endl;
        exit(1);
    }

    return servers;
}

std::vector<int> ParsePorts(std::vector<ServerConfig> servers) {
    std::vector<int> ports;
    for (const auto& server : servers) {
        ports.push_back(server.listen_port);
    }
    return ports;
}
