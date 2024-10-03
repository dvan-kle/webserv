#include "JsonParser.hpp"

void JsonParser::skipWhitespace() {
    // skips whitespace characters in the input string
    while (pos_ < input_.length() && std::isspace(input_[pos_])) {
        pos_++;
    }
}

std::string JsonParser::getNextString() {
    // skip any leading whitespace
    skipWhitespace();

    // check if we've reached the end of the input while expecting a quote
    if (pos_ >= input_.length()) {
        throw std::runtime_error("Error: Reached end of input while expecting '\"'");
    }

    // ensure the next character is a double quote for the start of the string
    if (input_[pos_] != '"') {
        throw std::runtime_error(std::string("Error: Expected '\"' but found '") + input_[pos_] + "' at position " + std::to_string(pos_));
    }

    // expect and move past the opening quote
    expect('"');
    // mark the start of the string
    size_t start = pos_;

    // move forward until we find the closing quote
    while (pos_ < input_.length() && input_[pos_] != '"') {
        pos_++;
    }
    // throw an error if the string is not terminated
    if (pos_ >= input_.length()) {
        throw std::runtime_error("Error: Unterminated string");
    }

    // extract the string between the quotes
    std::string result = input_.substr(start, pos_ - start);
    // move past the closing quote
    pos_++;

    return result;
}

int JsonParser::getNextInt() {
    // skip any leading whitespace
    skipWhitespace();

    // check if we've reached the end of the input
    if (pos_ >= input_.length()) {
        throw std::runtime_error("Error: Reached end of input while expecting a number");
    }

    // mark the start of the number
    size_t start = pos_;

    // check if the current character is a digit or a negative sign
    if (!std::isdigit(input_[pos_]) && input_[pos_] != '-') {
        throw std::runtime_error(std::string("Error: Expected a number but found '") + input_[pos_] + "' at position " + std::to_string(pos_));
    }

    // move the position forward while characters are digits
    while (pos_ < input_.length() && std::isdigit(input_[pos_])) {
        pos_++;
    }

    // extract the substring that represents the number
    std::string int_str = input_.substr(start, pos_ - start);

    try {
        // convert the string to an integer
        return std::stoi(int_str);
    } catch (const std::exception& e) {
        throw std::runtime_error("Error: Failed to convert string to integer");
    }
}

bool JsonParser::getNextBool() {
    // skip leading whitespace
    skipWhitespace();
    
    // check for "true" or "false" and return the corresponding boolean value
    if (input_.substr(pos_, 4) == "true") {
        // move past "true"
        pos_ += 4;
        return true;
    } else if (input_.substr(pos_, 5) == "false") {
        // move past "false"
        pos_ += 5;
        return false;
    } else {
        throw std::runtime_error("Error: Expected a boolean value");
    }
}

std::vector<std::string> JsonParser::getNextStringArray() {
    // skip leading whitespace
    skipWhitespace();
    
    // expect the opening '[' for the array
    expect('[');

    // store the resulting strings
    std::vector<std::string> result;

    while (true) {
        // parse each string in the array
        result.push_back(getNextString());
        // skip leading whitespace
        skipWhitespace();

        if (input_[pos_] == ',') {
            // move past the comma
            pos_++;
        } else if (input_[pos_] == ']') {
            // end of array
            pos_++;
            break;
        } else {
            throw std::runtime_error("Error: Expected ',' or ']' but found another character");
        }
    }

    return result;
}

void JsonParser::expect(char expected) {
    // skip leading whitespace
    skipWhitespace();

    // ensure we haven't reached the end of the input
    if (pos_ >= input_.length()) {
        throw std::runtime_error("Error: Reached end of input while expecting a character");
    }

    // check if the current character matches the expected one
    if (input_[pos_] != expected) {
        throw std::runtime_error("Error: Unexpected character");
    }

    // move past the expected character
    pos_++;
}

std::unordered_map<int, std::string> JsonParser::parseErrorPages() {
    std::unordered_map<int, std::string> error_pages;

    // expect the opening brace for the error pages object
    expect('{');

    while (true) {
        // skip leading whitespace
        skipWhitespace();

        // extract the error code as a string and convert it to an integer
        std::string code_str = getNextString();
        int code = std::stoi(code_str);

        // expect the colon separating the key and value
        expect(':');

        // get the page
        std::string page = getNextString();

        // store the error page mapping in the unordered map
        error_pages[code] = page;

        // skip leading whitespace
        skipWhitespace();

        if (input_[pos_] == ',') {
            // continue parsing the next error page
            pos_++;
        } else if (input_[pos_] == '}') {
            // end of error pages object
            pos_++;
            break;
        }
    }

    return error_pages;
}

LocationConfig JsonParser::parseLocationConfig() {
    LocationConfig loc;

    // expect the opening brace for the location config
    expect('{');

    while (true) {
        // skip leading whitespace
        skipWhitespace();

        // parse each key
        std::string key = getNextString();

        // expect the colon separator
        expect(':');

        // parse the value based on the key
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

        // skip leading whitespace
        skipWhitespace();


        if (input_[pos_] == ',') {
            // continue parsing the next key
            pos_++;
        } else if (input_[pos_] == '}') {
            // End of location config
            pos_++;
            break;
        }
    }

    return loc;
}

ServerConfig JsonParser::parseServerConfig() {
    ServerConfig server;

    // expect the opening brace for the server config
    expect('{');

    bool has_listen_host = false;
    bool has_listen_port = false;
    bool has_client_max_body_size = false;

    while (true) {
        // skip leading whitespace
        skipWhitespace();

        // parse each key
        std::string key = getNextString();

        // expect the colon separating the key and value
        expect(':');

        // parse based on the key
        if (key == "listen_host") {
            server.listen_host = getNextString();
            has_listen_host = true;  // Mark listen_host as provided
        } else if (key == "listen_port") {
            server.listen_port = getNextInt();
            has_listen_port = true;  // Mark listen_port as provided
        } else if (key == "server_name") {
            server.server_name = getNextString();
        } else if (key == "error_pages") {
            server.error_pages = parseErrorPages();
        } else if (key == "client_max_body_size") {
            server.client_max_body_size = getNextString();
            has_client_max_body_size = true;  // Mark client_max_body_size as provided
        } else if (key == "locations") {
            // start of locations array
            expect('[');

            while (input_[pos_] != ']') {
                // parse the next location configuration from the JSON and add it to the server's locations vector
                server.locations.push_back(parseLocationConfig());

                // skip leading whitespace
                skipWhitespace();

                if (input_[pos_] == ',') {
                    // continue parsing the next key
                    pos_++;
                }
            }
            // end of locations array
            expect(']');
        } else {
            throw std::runtime_error("Error: Unknown key in server config");
        }

        // skip leading whitespace
        skipWhitespace();

        if (input_[pos_] == ',') {
            // continue parsing the next key
            pos_++;
        } else if (input_[pos_] == '}') {
            // end of server config
            pos_++;
            break;
        }
    }

    // Validate required fields
    if (!has_listen_host) {
        throw std::runtime_error("Error: 'listen_host' is required but missing in server configuration.");
    }
    if (!has_listen_port) {
        throw std::runtime_error("Error: 'listen_port' is required but missing in server configuration.");
    }
    if (!has_client_max_body_size) {
        throw std::runtime_error("Error: 'client_max_body_size' is required but missing in server configuration.");
    }

    return server;
}

std::vector<ServerConfig> JsonParser::parse() {
    std::vector<ServerConfig> servers;

    // expect the root object to start with a brace
    expect('{');  

    while (true) {
        // skip leading whitespace
        skipWhitespace();

        // parse each key at the root level
        std::string key = getNextString();  

        // expect the colon separating the key and value
        expect(':');

        if (key == "servers") {
            // start of servers array
            expect('[');

            while (input_[pos_] != ']') {
                // parse the next server configuration from the JSON and add it to the server's vector
                servers.push_back(parseServerConfig());

                // skip leading whitespace
                skipWhitespace();

                if (input_[pos_] == ',') {
                    // continue parsing the next key
                    pos_++;
                }
            }
            // end of servers array
            expect(']');
        } else {
            throw std::runtime_error("Error: Unknown key at the root level");
        }

        // skip leading whitespace
        skipWhitespace();

        if (input_[pos_] == ',') {
            // continue parsing
            pos_++;
        } else if (input_[pos_] == '}') {
            // end of root object
            pos_++;
            break;
        }
    }

    return servers;
}
