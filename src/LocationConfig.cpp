#include "../include/Libaries.hpp"
#include "../include/LocationConfig.hpp"

// Getters
const std::string& LocationConfig::getPath() const { return path; }
const std::string& LocationConfig::getAlias() const { return alias; }
const std::string& LocationConfig::getIndex() const { return index; }
const std::string& LocationConfig::getReturnDirective() const { return return_directive; }
const std::string& LocationConfig::getRoot() const { return root; }
const std::string& LocationConfig::getClientBodyTempPath() const { return client_body_temp_path; }
const std::string& LocationConfig::getAutoindex() const { return autoindex; }
const std::set<std::string>& LocationConfig::getAcceptedMethods() const { return accepted_methods; }
const std::string& LocationConfig::getDefaultFile() const { return default_file; }
const std::set<std::string>& LocationConfig::getCgiExtensions() const { return cgi_extensions; }
const std::vector<std::string>& LocationConfig::getCgiPath() const { return cgi_path; }
const std::string& LocationConfig::getUploadPath() const { return upload_path; }

// Setters
void LocationConfig::setPath(const std::string& value) { path = value; }
void LocationConfig::setAlias(const std::string& value) { alias = value; }
void LocationConfig::setIndex(const std::string& value) { index = value; }
void LocationConfig::setReturnDirective(const std::string& value) { return_directive = value; }
void LocationConfig::setRoot(const std::string& value) { root = value; }
void LocationConfig::setClientBodyTempPath(const std::string& value) { client_body_temp_path = value; }
void LocationConfig::setAutoindex(const std::string& value) { autoindex = value; }
void LocationConfig::addAcceptedMethod(const std::string& value) { accepted_methods.insert(value); }
void LocationConfig::setDefaultFile(const std::string& value) { default_file = value; }
void LocationConfig::addCgiExtension(const std::string& value) { cgi_extensions.insert(value); }
void LocationConfig::addCgiPath(const std::string& value) { cgi_path.push_back(value); }
void LocationConfig::setUploadPath(const std::string& value) { upload_path = value; }
