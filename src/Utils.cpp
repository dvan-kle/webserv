#include "Request.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <regex>
#include <limits.h>
#include <sys/stat.h>

// get the current time in HTTP format
std::string Request::getCurrentTimeHttpFormat()
{
    // get the current time in seconds since epoch
    std::time_t now = std::time(nullptr);

    // convert to GMT but idk why we needed to do this
    std::tm *gmt_time = std::gmtime(&now);

    // use a string stream to format the time in the required HTTP date format
    std::ostringstream ss;
    ss << std::put_time(gmt_time, "%a, %d %b %Y %H:%M:%S GMT");

    // return the formatted date string
    return ss.str();
}

// find the best matching location for a given URL
LocationConfig* Request::findLocation(const std::string& url) {
    // initialize a pointer for the best match
    LocationConfig* best_match = nullptr;
    // keep track of the longest matching path length
    size_t best_match_length = 0;

    // iterate through the locations defined in the server config
    for (auto& location : _config.locations) {
        // check if the location path matches the beginning of the URL
        if (url.find(location.path) == 0) {
            size_t path_length = location.path.length();
            // update the best match if the current location path is longer
            if (path_length > best_match_length) {
                best_match = &location;
                best_match_length = path_length;
            }
        }
    }

    // if no match is found, fallback to the root location ("/")
    if (!best_match) {
        for (auto& location : _config.locations) {
            if (location.path == "/") {
                best_match = &location;
                break;
            }
        }
    }

    // return the best matching location
    return best_match;
}

// helper function to get the absolute path from a relative or absolute one
std::string Request::getAbsolutePath(const std::string &path) {
    // buffer to hold the absolute path
    char absPath[PATH_MAX];

    // if the path is already absolute (starts with '/'), return it as is
    if (path[0] == '/') {
        return path;
    }

    // otherwise, treat it as relative to the current working directory
    if (realpath(".", absPath) == nullptr) {
        std::cerr << "Error resolving current working directory!" << std::endl;
        // return the original path if we can't resolve the current directory
        return path;
    }

    // return the combined absolute path by appending the relative path to the current directory
    return std::string(absPath) + "/" + path;
}

// recursively create directories for a given path
void Request::createDir(const std::string &path) {
    // used to check the status of the path (e.g., whether it exists)
    struct stat st;
    // start with an empty string to build the path step-by-step
    std::string currentPath = "";
    // use a string stream to split the path into components
    std::istringstream pathStream(path);
    // to store each directory part as we iterate
    std::string directory;

    // check if the path is absolute (starts with a "/") and set the base path
    if (path[0] == '/') {
        // absolute path handling
        currentPath = "/";
    }

    // iterate through each directory in the provided path
    while (std::getline(pathStream, directory, '/')) {
        
        if (!directory.empty()) { // skip empty directory names (in case of multiple slashes)
            // append the current directory to the path
            currentPath += directory + "/";

            // check if the directory already exists using the stat function
            if (stat(currentPath.c_str(), &st) == -1) {  
                // directory doesn't exist, so we try to create it
                if (mkdir(currentPath.c_str(), 0755) == -1) {  // 0755 gives read/write/execute permissions for the owner, and read/execute for others
                    // if the directory cannot be created, serve a 500 Internal Server Error
                    std::cerr << "Failed to create directory: " << currentPath << std::endl;
                    ServeErrorPage(500);
                    return;
                }
            }
        }
    }
}
