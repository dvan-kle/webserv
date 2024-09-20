#include "../include/BodyParser.hpp"

std::string BodyParser::unchunkRequestBody(const std::string& buffer) {
    std::istringstream stream(buffer);
    std::string line;
    std::string body;

    while (std::getline(stream, line)) {
        int chunkSize;
        std::stringstream hexStream(line);
        hexStream >> std::hex >> chunkSize;
        if (chunkSize == 0) {
            break;  // End of chunked data
        }

        char* chunkData = new char[chunkSize + 1];
        stream.read(chunkData, chunkSize);
        chunkData[chunkSize] = '\0';
        body += std::string(chunkData);
        delete[] chunkData;

        std::getline(stream, line);  // Skip the CRLF after the chunk
    }
    return body;
}
