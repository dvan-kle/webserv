#include "../include/JsonParser.hpp"

void JsonParser::skipWhitespace() {
    while (pos_ < input_.length() && std::isspace(input_[pos_]))
        pos_++;
}

char JsonParser::peek() const {
    if (pos_ < input_.length())
        return input_[pos_];
    return '\0';
}

char JsonParser::get() {
    if (pos_ < input_.length())
        return input_[pos_++];
    return '\0';
}

JsonValue JsonParser::parseValue() {
    skipWhitespace();
    char c = peek();
    if (c == 'n')
        return parseNull();
    if (c == 't' || c == 'f')
        return parseBool();
    if (c == '-' || std::isdigit(c))
        return parseNumber();
    if (c == '"')
        return parseString();
    if (c == '[')
        return parseArray();
    if (c == '{')
        return parseObject();

    throw std::runtime_error(std::string("Unexpected character: ") + c);
}

JsonValue JsonParser::parseNull() {
    expect("null");
    return JsonValue();  // Null value
}

JsonValue JsonParser::parseBool() {
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

JsonValue JsonParser::parseNumber() {
    size_t start_pos = pos_;
    if (peek() == '-')
        pos_++;
    if (peek() == '0')
        pos_++;
    else if (std::isdigit(peek()))
        while (std::isdigit(peek())) pos_++;
    else
        throw std::runtime_error("Invalid number");
    if (peek() == '.') {
        pos_++;
        if (!std::isdigit(peek()))
            throw std::runtime_error("Invalid number");
        while (std::isdigit(peek()))
            pos_++;
    }
    if (peek() == 'e' || peek() == 'E') {
        pos_++;
        if (peek() == '+' || peek() == '-')
            pos_++;
        if (!std::isdigit(peek()))
            throw std::runtime_error("Invalid number");
        while (std::isdigit(peek()))
            pos_++;
    }
    std::string number_str = input_.substr(start_pos, pos_ - start_pos);
    JsonValue value;
    value.type = JsonType::Number;
    value.number_value = std::stod(number_str);
    return value;
}

JsonValue JsonParser::parseString() {
    expect("\"");
    std::string result;
    while (true) {
        if (pos_ >= input_.length())
            throw std::runtime_error("Unterminated string");
        char c = get();
        if (c == '"')
            break;
        else if (c == '\\') {
            if (pos_ >= input_.length())
                throw std::runtime_error("Unterminated string");
            c = get();
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

JsonValue JsonParser::parseArray() {
    expect("[");
    JsonValue value;
    value.type = JsonType::Array;
    skipWhitespace();
    if (peek() == ']') {
        get();  // Consume ']'
        return value;
    }
    while (true) {
        JsonValue element = parseValue();
        value.array_value.push_back(element);
        skipWhitespace();
        if (peek() == ',') {
            get();  // Consume ','
            skipWhitespace();
        } else if (peek() == ']') {
            get();  // Consume ']'
            break;
        } else
            throw std::runtime_error("Expected ',' or ']'");
    }
    return value;
}

JsonValue JsonParser::parseObject() {
    expect("{");
    JsonValue value;
    value.type = JsonType::Object;
    skipWhitespace();
    if (peek() == '}') {
        get();  // Consume '}'
        return value;
    }
    while (true) {
        skipWhitespace();
        if (peek() != '"')
            throw std::runtime_error("Expected string key");
        JsonValue key = parseString();
        skipWhitespace();
        expect(":");
        JsonValue val = parseValue();
        value.object_value[key.string_value] = val;
        skipWhitespace();
        if (peek() == ',') {
            get();  // Consume ','
            skipWhitespace();
        } else if (peek() == '}') {
            get();  // Consume '}'
            break;
        } else
            throw std::runtime_error("Expected ',' or '}'");
    }
    return value;
}

void JsonParser::expect(const std::string& expected) {
    for (char c : expected) {
        if (get() != c)
            throw std::runtime_error(std::string("Expected '") + expected + "'");
    }
}

JsonValue JsonParser::parse() {
    skipWhitespace();
    JsonValue value = parseValue();
    skipWhitespace();
    if (pos_ != input_.length())
        throw std::runtime_error("Extra characters after JSON data");
    return value;
}

// Function to display the parsed configuration (for testing purposes)
void displayConfig(const std::vector<ServerConfig>& servers) {
    for (const auto& server : servers) {
        std::cout << "Server:" << std::endl;
        std::cout << "  Listen: " << server.listen_host << ":" << server.listen_port << std::endl;
        std::cout << "  Server Name: " << server.server_name << std::endl;
        std::cout << "  Client Max Body Size: " << server.client_max_body_size << std::endl;

        std::cout << "  Error Pages:" << std::endl;
        for (const auto& error_page : server.error_pages) {
            std::cout << "    " << error_page.first << " => " << error_page.second << std::endl;
        }

        std::cout << "  Locations:" << std::endl;
        for (const auto& loc : server.locations) {
            std::cout << "    Location: " << loc.path << std::endl;
            std::cout << "      Methods: ";
            for (const auto& method : loc.methods) {
                std::cout << method << " ";
            }
            std::cout << std::endl;
            if (!loc.redirection.empty())
                std::cout << "      Redirection: " << loc.redirection << std::endl;
            if (loc.return_code != 0)
                std::cout << "      Return Code: " << loc.return_code << std::endl;
            if (!loc.root.empty())
                std::cout << "      Root: " << loc.root << std::endl;
            std::cout << "      Autoindex: " << (loc.autoindex ? "On" : "Off") << std::endl;
            if (!loc.index.empty())
                std::cout << "      Index: " << loc.index << std::endl;
            if (!loc.upload_path.empty())
                std::cout << "      Upload Path: " << loc.upload_path << std::endl;
            if (!loc.cgi_extension.empty())
                std::cout << "      CGI Extension: " << loc.cgi_extension << std::endl;
            if (!loc.cgi_path.empty())
                std::cout << "      CGI Path: " << loc.cgi_path << std::endl;
        }
        std::cout << std::endl;
    }
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

void parseConfig(int argc, char* argv[]) {
    std::string config_file = "webserv.json";
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
        json_root = parser.parse();
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

    // Display the parsed configuration (for testing)
    displayConfig(servers);
}
