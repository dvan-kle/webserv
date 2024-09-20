#pragma once

#include "Libaries.hpp"

class AutoIndex {
	public:
    	static std::string generateDirectoryListing(const std::string& directoryPath, const std::string& url, const std::string& host, int port);
};
