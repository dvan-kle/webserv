#include "JsonParser.hpp"

void JsonParser::skipWhitespace() {
    while (pos_ < input_.length() && std::isspace(input_[pos_])) {
        pos_++;
    }
}

std::string JsonParser::getNextString() {
    skipWhitespace();  // Ensure no leading whitespace

    if (pos_ >= input_.length()) {
        throw std::runtime_error("Error: Reached end of input while expecting '\"'");
    }

    if (input_[pos_] != '"') {
        throw std::runtime_error(std::string("Error: Expected '\"' but found '") + input_[pos_] + "' at position " + std::to_string(pos_));
    }

    expect('"');  // Expect opening quote
    size_t start = pos_;  // Start just after the quote

    while (pos_ < input_.length() && input_[pos_] != '"') {
        pos_++;
    }

    if (pos_ >= input_.length()) {
        throw std::runtime_error("Error: Unterminated string starting at position " + std::to_string(start));
    }

    std::string result = input_.substr(start, pos_ - start);
    pos_++;  // Move past the closing quote

    return result;
}

int JsonParser::getNextInt() {
    skipWhitespace();

    if (pos_ >= input_.length()) {
        throw std::runtime_error("Error: Reached end of input while expecting a number");
    }

    size_t start = pos_;
    if (!std::isdigit(input_[pos_]) && input_[pos_] != '-') {
        throw std::runtime_error(std::string("Error: Expected a number but found '") + input_[pos_] + "' at position " + std::to_string(pos_));
    }

    while (pos_ < input_.length() && std::isdigit(input_[pos_])) {
        pos_++;
    }

    std::string int_str = input_.substr(start, pos_ - start);

    try {
        return std::stoi(int_str);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Error: Failed to convert '") + int_str + "' to an integer at position " + std::to_string(start));
    }
}

bool JsonParser::getNextBool() {
    skipWhitespace();
    if (input_.substr(pos_, 4) == "true") {
        pos_ += 4;
        return true;
    } else if (input_.substr(pos_, 5) == "false") {
        pos_ += 5;
        return false;
    } else {
        throw std::runtime_error("Error: Expected boolean value (true or false) at position " + std::to_string(pos_));
    }
}

std::vector<std::string> JsonParser::getNextStringArray() {
    skipWhitespace();
    expect('[');  // Expect the opening '['
    std::vector<std::string> result;

    while (true) {
        result.push_back(getNextString());
        skipWhitespace();

        // Handle trailing comma or end of array
        if (input_[pos_] == ',') {
            pos_++;  // Move past comma and continue
            skipWhitespace();
        } else if (input_[pos_] == ']') {
            pos_++;  // End of array
            break;
        } else {
            throw std::runtime_error("Error: Expected ',' or ']' but found '" + std::string(1, input_[pos_]) + "' at position " + std::to_string(pos_));
        }
    }

    return result;
}

void JsonParser::expect(char expected) {
    skipWhitespace();

    if (pos_ >= input_.length()) {
        throw std::runtime_error(std::string("Error: Reached end of input while expecting '") + expected + "'");
    }

    if (input_[pos_] != expected) {
        throw std::runtime_error(std::string("Error: Expected '") + expected + "' but found '" + input_[pos_] + "' at position " + std::to_string(pos_));
    }

    pos_++;
}

std::unordered_map<int, std::string> JsonParser::parseErrorPages() {
    expect('{');  // Start of error pages object
    std::unordered_map<int, std::string> error_pages;

    while (true) {
        skipWhitespace();

        // Parse key as a string first
        std::string code_str = getNextString();
        
        // Convert the string key to an integer using stoi
        int code;
        try {
            code = std::stoi(code_str);
        } catch (const std::exception& e) {
            throw std::runtime_error("Error: Failed to convert error code '" + code_str + "' to an integer");
        }

        skipWhitespace();
        expect(':');
        std::string page = getNextString();
        error_pages[code] = page;

        skipWhitespace();
        if (input_[pos_] == ',') {
            pos_++;  // Continue to next key-value pair
        } else if (input_[pos_] == '}') {
            pos_++;  // End of object
            break;
        } else {
            throw std::runtime_error("Error: Expected ',' or '}' in error pages, but found '" + std::string(1, input_[pos_]) + "' at position " + std::to_string(pos_));
        }
    }

    return error_pages;
}

LocationConfig JsonParser::parseLocationConfig() {
    LocationConfig loc;
    bool has_path = false;
    bool has_methods = false;

    expect('{');

    while (true) {
        skipWhitespace();
        std::string key = getNextString();
        expect(':');

        if (key == "path") {
            loc.path = getNextString();
            has_path = true;
        } else if (key == "methods") {
            loc.methods = getNextStringArray();
            has_methods = true;
        } else if (key == "root") {
            loc.root = getNextString();
        } else if (key == "autoindex") {
            loc.autoindex = getNextBool();
        } else if (key == "redirection") {
            loc.redirection = getNextString();
        } else if (key == "return_code") {
            loc.return_code = getNextInt();
        } else if (key == "cgi_extension") {
            loc.cgi_extension = getNextStringArray();
        } else if (key == "cgi_path") {
            loc.cgi_path = getNextStringArray();
        } else if (key == "upload_path") {
            loc.upload_path = getNextString();
        } else if (key == "index") {
            loc.index = getNextString();
        } else {
            throw std::runtime_error("Error: Unknown key '" + key + "' in location config");
        }

        skipWhitespace();
        if (input_[pos_] == ',') {
            pos_++;
        } else if (input_[pos_] == '}') {
            pos_++;  // End of object
            break;
        } else {
            throw std::runtime_error("Error: Expected ',' or '}' but found '" + std::string(1, input_[pos_]) + "' in location config at position " + std::to_string(pos_));
        }
    }

    // Check for missing required fields
    if (!has_path) {
        throw std::runtime_error("Error: Missing required field 'path' in location config");
    }
    if (!has_methods) {
        throw std::runtime_error("Error: Missing required field 'methods' in location config");
    }

    return loc;
}

ServerConfig JsonParser::parseServerConfig() {
    ServerConfig server;
    bool has_listen_host = false;
    bool has_listen_port = false;

    expect('{');

    while (true) {
        skipWhitespace();
        std::string key = getNextString();
        expect(':');

        if (key == "listen_host") {
            server.listen_host = getNextString();
            has_listen_host = true;
        } else if (key == "listen_port") {
            server.listen_port = getNextInt();
            has_listen_port = true;
        } else if (key == "server_name") {
            server.server_name = getNextString();
        } else if (key == "error_pages") {
            server.error_pages = parseErrorPages();
        } else if (key == "client_max_body_size") {
            server.client_max_body_size = getNextString();
        } else if (key == "locations") {
            expect('[');  // Start of locations array
            while (input_[pos_] != ']') {
                server.locations.push_back(parseLocationConfig());
                skipWhitespace();
                if (input_[pos_] == ',') {
                    pos_++;
                }
            }
            expect(']');  // End of locations array
        } else {
            throw std::runtime_error("Error: Unknown key '" + key + "' in server config");
        }

        skipWhitespace();
        if (input_[pos_] == ',') {
            pos_++;
        } else if (input_[pos_] == '}') {
            pos_++;  // End of object
            break;
        } else {
            throw std::runtime_error("Error: Expected ',' or '}' but found '" + std::string(1, input_[pos_]) + "' in server config at position " + std::to_string(pos_));
        }
    }

    // Check for missing required fields
    if (!has_listen_host) {
        throw std::runtime_error("Error: Missing required field 'listen_host' in server config");
    }
    if (!has_listen_port) {
        throw std::runtime_error("Error: Missing required field 'listen_port' in server config");
    }

    return server;
}


std::vector<ServerConfig> JsonParser::parse() {
    std::vector<ServerConfig> servers;
    bool has_servers = false;

    expect('{');  // Expect the start of the root object

    while (true) {
        skipWhitespace();
        std::string key = getNextString();
        expect(':');

        if (key == "servers") {
            has_servers = true;
            expect('[');  // Start of servers array
            while (input_[pos_] != ']') {
                servers.push_back(parseServerConfig());
                skipWhitespace();
                if (input_[pos_] == ',') {
                    pos_++;
                }
            }
            expect(']');  // End of servers array
        } else {
            throw std::runtime_error("Error: Unknown key '" + key + "' at the root level");
        }

        skipWhitespace();
        if (input_[pos_] == ',') {
            pos_++;
        } else if (input_[pos_] == '}') {
            pos_++;  // End of object
            break;
        }
    }

    // Check for missing required fields
    if (!has_servers) {
        throw std::runtime_error("Error: Missing required field 'servers' in root object");
    }

    return servers;
}
