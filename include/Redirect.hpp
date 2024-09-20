#pragma once

#include "Libaries.hpp"

class Redirect {
	public:
    	static void sendRedirectResponse(int client_fd, const std::string& http_version, const std::string& redirection_url, int return_code, const std::string& server_name);
};
