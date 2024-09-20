#pragma once

#include "Libaries.hpp"

class WriteClient {
public:
    static void safeWriteToClient(int client_fd, const std::string& response);
};
