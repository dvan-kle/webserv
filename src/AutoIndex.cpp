#include "../include/AutoIndex.hpp"

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
        if (entryName == ".") continue;  // Skip current directory

        std::string entryUrl = "http://" + host + ":" + std::to_string(port) + url + (entryName == ".." ? "/.." : "/" + entryName);
        html << "<li><a href=\"" << entryUrl << "\">" << entryName << "</a></li>";
    }

    closedir(dir);
    html << "</ul></body></html>";

    return html.str();
}
