#ifndef LOCATIONCONFIG_HPP
#define LOCATIONCONFIG_HPP

#include "Libaries.hpp"

class LocationConfig {
    private:
        std::string path;
        std::string alias;
        std::string index;
        std::string return_directive;
        std::string root;
        std::string client_body_temp_path;
        std::string autoindex;
        std::set<std::string> accepted_methods;
        std::string default_file;
        std::set<std::string> cgi_extensions;
        std::vector<std::string> cgi_path;
        std::string upload_path;

    public:
        // Getters
        const std::string& getPath() const;
        const std::string& getAlias() const;
        const std::string& getIndex() const;
        const std::string& getReturnDirective() const;
        const std::string& getRoot() const;
        const std::string& getClientBodyTempPath() const;
        const std::string& getAutoindex() const;
        const std::set<std::string>& getAcceptedMethods() const;
        const std::string& getDefaultFile() const;
        const std::set<std::string>& getCgiExtensions() const;
        const std::vector<std::string>& getCgiPath() const;
        const std::string& getUploadPath() const;

        // Setters
        void setPath(const std::string& value);
        void setAlias(const std::string& value);
        void setIndex(const std::string& value);
        void setReturnDirective(const std::string& value);
        void setRoot(const std::string& value);
        void setClientBodyTempPath(const std::string& value);
        void setAutoindex(const std::string& value);
        void addAcceptedMethod(const std::string& value);
        void setDefaultFile(const std::string& value);
        void addCgiExtension(const std::string& value);
        void addCgiPath(const std::string& value);
        void setUploadPath(const std::string& value);
};

#endif
