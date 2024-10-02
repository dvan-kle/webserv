#include "JsonParser.hpp"

void JsonParser::skipWhitespace() {
    // Skips whitespace characters in the input string
    while (pos_ < input_.length() && std::isspace(input_[pos_])) {
        pos_++;
    }
}

std::string JsonParser::getNextString() {
    skipWhitespace();  // Skip any leading whitespace

    if (pos_ >= input_.length()) {
        throw std::runtime_error("Error: Reached end of input while expecting '\"'");
    }

    if (input_[pos_] != '"') {
        throw std::runtime_error(std::string("Error: Expected '\"' but found '") + input_[pos_] + "' at position " + std::to_string(pos_));
    }

    expect('"');  // Expect and move past the opening quote
    size_t start = pos_;  // Mark the start of the string

    while (pos_ < input_.length() && input_[pos_] != '"') {
        pos_++;
    }

    if (pos_ >= input_.length()) {
        throw std::runtime_error("Error: Unterminated string");
    }

    std::string result = input_.substr(start, pos_ - start);
    pos_++;  // Move past the closing quote

    return result;
}

int JsonParser::getNextInt() {
    skipWhitespace();

    // Check if we've reached the end of input
    if (pos_ >= input_.length()) {
        throw std::runtime_error("Error: Reached end of input while expecting a number");
    }

    size_t start = pos_;  // Mark the start of the number

    // Check for valid numeric characters
    if (!std::isdigit(input_[pos_]) && input_[pos_] != '-') {
        throw std::runtime_error(std::string("Error: Expected a number but found '") + input_[pos_] + "' at position " + std::to_string(pos_));
    }

    // Move the position forward through digits
    while (pos_ < input_.length() && std::isdigit(input_[pos_])) {
        pos_++;
    }

    // Convert the substring to an integer
    std::string int_str = input_.substr(start, pos_ - start);

    try {
        return std::stoi(int_str);
    } catch (const std::exception& e) {
        throw std::runtime_error("Error: Failed to convert string to integer");
    }
}

bool JsonParser::getNextBool() {
    skipWhitespace();
    
    // Check for "true" or "false" literals and return the corresponding boolean value
    if (input_.substr(pos_, 4) == "true") {
        pos_ += 4;
        return true;
    } else if (input_.substr(pos_, 5) == "false") {
        pos_ += 5;
        return false;
    } else {
        throw std::runtime_error("Error: Expected a boolean value");
    }
}

std::vector<std::string> JsonParser::getNextStringArray() {
    skipWhitespace();
    expect('[');  // Expect the opening '[' for the array

    std::vector<std::string> result;

    while (true) {
        result.push_back(getNextString());  // Parse each string in the array
        skipWhitespace();

        if (input_[pos_] == ',') {
            pos_++;  // Move past the comma
        } else if (input_[pos_] == ']') {
            pos_++;  // End of array
            break;
        } else {
            throw std::runtime_error("Error: Expected ',' or ']' but found another character");
        }
    }

    return result;
}

void JsonParser::expect(char expected) {
    skipWhitespace();  // Skip any leading whitespace

    if (pos_ >= input_.length()) {
        throw std::runtime_error("Error: Reached end of input while expecting a character");
    }

    if (input_[pos_] != expected) {
        throw std::runtime_error("Error: Unexpected character");
    }

    pos_++;  // Move past the expected character
}

std::unordered_map<int, std::string> JsonParser::parseErrorPages() {
    expect('{');  // Expect the opening brace for the error pages object

    std::unordered_map<int, std::string> error_pages;

    while (true) {
        skipWhitespace();

        // Extract the error code as a string and convert it to an integer
        std::string code_str = getNextString();
        int code = std::stoi(code_str);

        expect(':');  // Expect the colon separating the key and value
        std::string page = getNextString();

        error_pages[code] = page;  // Store the error page mapping

        skipWhitespace();

        if (input_[pos_] == ',') {
            pos_++;  // Continue parsing the next error page
        } else if (input_[pos_] == '}') {
            pos_++;  // End of error pages object
            break;
        }
    }

    return error_pages;
}

LocationConfig JsonParser::parseLocationConfig() {
    LocationConfig loc;
    expect('{');  // Expect the opening brace for the location config

    while (true) {
        skipWhitespace();
        std::string key = getNextString();  // Parse each key
        expect(':');  // Expect the colon separator

        // Parse based on the key
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
            throw std::runtime_error("Error: Unknown key in location config");
        }

        skipWhitespace();
        if (input_[pos_] == ',') {
            pos_++;
        } else if (input_[pos_] == '}') {
            pos_++;  // End of location config
            break;
        }
    }

    return loc;
}

ServerConfig JsonParser::parseServerConfig() {
    ServerConfig server;
    expect('{');  // Expect the opening brace for the server config

    while (true) {
        skipWhitespace();
        std::string key = getNextString();  // Parse each key
        expect(':');

        // Parse based on the key
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
        } else {
            throw std::runtime_error("Error: Unknown key in server config");
        }

        skipWhitespace();
        if (input_[pos_] == ',') {
            pos_++;
        } else if (input_[pos_] == '}') {
            pos_++;  // End of server config
            break;
        }
    }

    return server;
}

std::vector<ServerConfig> JsonParser::parse() {
    std::vector<ServerConfig> servers;
    expect('{');  // Expect the root object to start with a brace

    while (true) {
        skipWhitespace();
        std::string key = getNextString();  // Parse each key at the root level
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
        } else {
            throw std::runtime_error("Error: Unknown key at the root level");
        }

        skipWhitespace();
        if (input_[pos_] == ',') {
            pos_++;
        } else if (input_[pos_] == '}') {
            pos_++;  // End of root object
            break;
        }
    }

    return servers;
}
