#include "../include/BodyParser.hpp"

std::string BodyParser::unchunkRequestBody(const std::string& buffer) {
    std::istringstream stream(buffer);
    std::string line;
    std::string body;

    while (std::getline(stream, line)) {
        if (line.empty()) continue;  // Skip empty lines
        int chunkSize = 0;
        std::stringstream hexStream;
        hexStream << std::hex << line;
        hexStream >> chunkSize;

        if (chunkSize <= 0) {
            // Read and discard any trailing headers
            while (std::getline(stream, line) && !line.empty()) {}
            break;  // End of chunked data
        }

        // Read the chunk data
        char* chunkData = new char[chunkSize];
        stream.read(chunkData, chunkSize);
        body.append(chunkData, chunkSize);
        delete[] chunkData;

        // Read the trailing CRLF after the chunk data
        stream.ignore(2);
    }
    return body;
}
