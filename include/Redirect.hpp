#pragma once

#include <string>

class Redirect {
	public:
    	static std::string generateRedirectResponse(const std::string& http_version, const std::string& redirection_url, int return_code, const std::string& server_name);
};
