#include "../include/Request.hpp"
#include <sstream>
#include <dirent.h>
#include <sys/stat.h>

// generates an HTML directory listing and sends it as a response
void Request::ServeAutoIndex(const std::string& directoryPath, const std::string& url, const std::string& host, int port, LocationConfig* location) {

    // adjust directoryPath to include the additional part of the URL after the location's path
    std::string adjustedDirectoryPath = directoryPath;

    // if the URL is not the same as the location's path, append the remaining URL to the directoryPath
    if (url != location->path && url.find(location->path) == 0) {
        std::string remainingUrl = url.substr(location->path.length());
        if (!remainingUrl.empty()) {
            if (remainingUrl[0] == '/') {
                remainingUrl = remainingUrl.substr(1);
            }
            adjustedDirectoryPath += "/" + remainingUrl;
        }
    }

    // open the directory specified by adjustedDirectoryPath
    DIR* dir = opendir(adjustedDirectoryPath.c_str());
    if (dir == nullptr) {
        std::cerr << "Error: Unable to open directory: " << adjustedDirectoryPath << std::endl;
        ServeErrorPage(500);
        return;
    }

    // create a string stream to build the HTML response
    std::ostringstream html;
    
    // start HTML construction
    html << "<html><head><title>Directory Listing</title></head><body>";
    html << "<h1>Index of " << url << "</h1>";
    html << "<ul>";

    // loop through the entries in the directory
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string entryName = entry->d_name;

        // skip the current directory (".") and parent directory ("..")
        if (entryName == "." || entryName == "..") {
            continue;
        }

        // construct the full path to the current directory entry
        std::string fullPath = adjustedDirectoryPath + "/" + entryName;

        struct stat entryStat;
        // use the 'stat' system call to get information about the file or directory at 'fullPath'
        if (stat(fullPath.c_str(), &entryStat) == -1) {
            std::cerr << "Error: Failed to stat file: " << fullPath << std::endl;
            continue;
        }

        // if it's a directory, append a slash to the name
        if (S_ISDIR(entryStat.st_mode)) {
            entryName += "/";
        }

        // show entries
        html << "<li>" << entryName << "</li>";
    }

    // close the directory after reading its contents
    closedir(dir);

    // close HTML construction
    html << "</ul></body></html>";

    std::string htmlContent = html.str();

    // add the response header with HTTP 200 OK status and the correct content length
    responseHeader(htmlContent, HTTP_200);

    // append the generated HTML to the response
    _response += htmlContent;
}
