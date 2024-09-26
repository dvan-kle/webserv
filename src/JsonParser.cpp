#include "JsonParser.hpp"

void JsonParser::skipWhitespace() {
    while (pos_ < input_.length() && std::isspace(input_[pos_])) {
        pos_++;
    }
}

std::string JsonParser::getNextString() {
    skipWhitespace();  // Ensure no leading whitespace

    if (pos_ >= input_.length()) {
        throw std::runtime_error("Reached end of input while expecting '\"'");
    }

    if (input_[pos_] != '"') {
        throw std::runtime_error("Expected '\"'");
    }

    expect('"');  // Expect opening quote
    size_t start = pos_;  // Start just after the quote

    while (pos_ < input_.length() && input_[pos_] != '"') {
        pos_++;
    }

    if (pos_ >= input_.length()) {
        throw std::runtime_error("Unterminated string");
    }

    std::string result = input_.substr(start, pos_ - start);
    pos_++;  // Move past the closing quote

    return result;
}

int JsonParser::getNextInt() {
    skipWhitespace();

    if (pos_ >= input_.length()) {
        throw std::runtime_error("Reached end of input while expecting a number");
    }

    size_t start = pos_;
    if (!std::isdigit(input_[pos_]) && input_[pos_] != '-') {
        throw std::runtime_error("Expected a number");
    }

    while (pos_ < input_.length() && std::isdigit(input_[pos_])) {
        pos_++;
    }

    std::string int_str = input_.substr(start, pos_ - start);

    try {
        return std::stoi(int_str);
    } catch (const std::exception& e) {
        throw;
    }
}


// Helper function to get the next boolean value.
bool JsonParser::getNextBool() {
    skipWhitespace();
    if (input_.substr(pos_, 4) == "true") {
        pos_ += 4;
        return true;
    } else if (input_.substr(pos_, 5) == "false") {
        pos_ += 5;
        return false;
    } else {
        throw std::runtime_error("Expected boolean value");
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
            throw std::runtime_error("Expected ',' or ']'");
        }
    }

    return result;
}


void JsonParser::expect(char expected) {
    skipWhitespace();

    if (pos_ >= input_.length()) {
        throw std::runtime_error("Reached end of input while expecting character");
    }

    if (input_[pos_] != expected) {
        throw std::runtime_error(std::string("Expected '") + expected + "'");
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
            throw;
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
            throw std::runtime_error("Expected ',' or '}' in error pages");
        }
    }
    
    return error_pages;
}


LocationConfig JsonParser::parseLocationConfig() {
    LocationConfig loc;
    expect('{');

    while (true) {
        skipWhitespace();
        std::string key = getNextString();
        expect(':');

        if (key == "path") {
            loc.path = getNextString();
        } else if (key == "methods") {
            loc.methods = getNextStringArray();
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
            throw std::runtime_error("Unknown key in location config");
        }

        skipWhitespace();
        if (input_[pos_] == ',') {
            pos_++;
        } else if (input_[pos_] == '}') {
            pos_++;
            break;
        } else {
            throw std::runtime_error("Expected ',' or '}'");
        }
    }

    return loc;
}


// Parse a single server configuration object.
ServerConfig JsonParser::parseServerConfig() {
    ServerConfig server;
    expect('{');

    while (true) {
        skipWhitespace();
        std::string key = getNextString();
        expect(':');

        if (key == "listen_host") {
            server.listen_host = getNextString();
        } else if (key == "listen_port") {
            server.listen_port = getNextInt();
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
        }

        skipWhitespace();
        if (input_[pos_] == ',') {
            pos_++;
        } else if (input_[pos_] == '}') {
            pos_++;
            break;
        }
    }

    return server;
}

// Main parser method that parses the whole configuration.
std::vector<ServerConfig> JsonParser::parse() {
    std::vector<ServerConfig> servers;
    expect('{');  // Expect the start of the root object

    while (true) {
        skipWhitespace();
        std::string key = getNextString();
        expect(':');

        if (key == "servers") {
            expect('[');  // Start of servers array
            while (input_[pos_] != ']') {
                servers.push_back(parseServerConfig());
                skipWhitespace();
                if (input_[pos_] == ',') {
                    pos_++;
                }
            }
            expect(']');  // End of servers array
        }

        skipWhitespace();
        if (input_[pos_] == ',') {
            pos_++;
        } else if (input_[pos_] == '}') {
            pos_++;
            break;
        }
    }

    return servers;
}
