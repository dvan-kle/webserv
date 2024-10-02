#include "../include/Request.hpp"
#include <sstream>
#include <dirent.h>

std::string Request::ServeAutoIndex(const std::string& directoryPath, const std::string& url, const std::string& host, int port) {
    // create a string stream to build the HTML response
    std::ostringstream html;
    
    // start html construction
    html << "<html><head><title>Directory Listing</title></head><body>";
    html << "<h1>Index of " << url << "</h1>";
    html << "<ul>";

    // open the directory specified by directoryPath
    DIR* dir = opendir(directoryPath.c_str());
    if (dir == nullptr) {
        return "<html><body>Error: Unable to open directory</body></html>";
    }

    // loop through the entries in the directory
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string entryName = entry->d_name;

        // skip the current directory (".")
        if (entryName == ".")
            continue;

        html << "<li>" << entryName << "</li>";
    }

    // close the directory after reading its contents
    closedir(dir);
    
    // close html construction
    html << "</ul></body></html>";

    // return the complete HTML directory listing as a string
    return html.str();
}
