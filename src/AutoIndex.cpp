#include "../include/AutoIndex.hpp"
#include <sstream>
#include <dirent.h>

std::string AutoIndex::generateDirectoryListing(const std::string& directoryPath, const std::string& url, const std::string& host, int port) {
    std::ostringstream html;
    html << "<html><head><title>Directory Listing</title></head><body>";
    html << "<h1>Index of " << url << "</h1>";
    html << "<ul>";

    DIR* dir = opendir(directoryPath.c_str());
    if (dir == nullptr) {
        return "<html><body>Error: Unable to open directory</body></html>";
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string entryName = entry->d_name;
        if (entryName == ".")
            continue;  // Skip current directory

        html << "<li>" << entryName << "</li>";
    }

    closedir(dir);
    html << "</ul></body></html>";

    return html.str();
}
