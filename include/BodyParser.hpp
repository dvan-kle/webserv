#pragma once

#include "Libaries.hpp"

class BodyParser {
public:
    static std::string unchunkRequestBody(const std::string& buffer);
};
