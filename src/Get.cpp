#include "Request.hpp"
#include "Header.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <iterator>
#include <regex>

void Request::HandleGetRequest() {
	// find the location/url block for the given URL
    auto location = findLocation(_url);

	// serve 404 error if location is not found
    if (location == nullptr) {
        ServeErrorPage(404);
        return;
    }

	// start with the root path from the location config
    std::string filePath = location->root;

    // if the URL is not the same as the location path, append the URL to the file path
    if (_url != location->path) {
        filePath += _url;
    }

	// serve either a file or directory depending on the constructed file path
    ServeFileOrDirectory(filePath, location);
}

void Request::ServeFileOrDirectory(const std::string &filePath, LocationConfig* location) {
    struct stat pathStat;
	// check if the file path exists and get its status
    if (stat(filePath.c_str(), &pathStat) == -1) {
		// serve 404 if the path doesn't exist
        ServeErrorPage(404);
        return;
    }

	// if the path is a directory, handle the directory request
    if (S_ISDIR(pathStat.st_mode)) {
        HandleDirectoryRequest(filePath, location);
    } else {
		// otherwise, serve the file
        ServeFile(filePath);
    }
}

void Request::HandleDirectoryRequest(const std::string &filePath, LocationConfig* location) {
    // if the location config specifies an index file
    if (!location->index.empty()) {
        std::string fullPath = filePath;

		// ensure directory paths end with a "/"
        if (fullPath.back() != '/')
			fullPath += '/';
		
		// append the index file to the directory path
        fullPath += location->index;

        struct stat fileStat;
		// check if the index file exists and is a regular file
        if (stat(fullPath.c_str(), &fileStat) == -1 || !S_ISREG(fileStat.st_mode)) {
			// if the index file doesn't exist, check if autoindex is enabled
            if (location->autoindex) {
				// serve an generated directory listing
                ServeAutoIndex(location->root, _url, _config.listen_host, _config.listen_port, location);
            } else {
				// serve 404 if autoindex is disabled and no index file is found
                ServeErrorPage(404);
            }
        } else {
			// serve the index file if it exists
            ServeFile(fullPath);
        }
    } else if (location->autoindex) {
		// if autoindex is enabled but no index file is specified, serve the directory listing
        ServeAutoIndex(location->root, _url, _config.listen_host, _config.listen_port, location);
    } else {
		// serve 404 if neither index nor autoindex is available
        ServeErrorPage(404);
    }
}

void Request::ServeFile(const std::string &filePath) {
	// open the file in binary mode
    std::ifstream ifstr(filePath, std::ios::binary);
    if (!ifstr) {
		// serve 404 if the file can't be opened
        ServeErrorPage(404);
        return;
    }

	// read the file contents into a string
    std::string content((std::istreambuf_iterator<char>(ifstr)), std::istreambuf_iterator<char>());

	// add the response header with HTTP 200 OK status
    responseHeader(content, HTTP_200);
	// append the file content to the response
    _response += content;
}
