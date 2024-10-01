#pragma once

#include <string>

class BodyParser {
public:
    static std::string unchunkRequestBody(const std::string& buffer);
};
