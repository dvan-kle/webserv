#pragma once

#include "JsonParser.hpp"
#include <string>
#include <vector>

const std::string HTTP_200 = "200 OK";
const std::string HTTP_400 = "400 Bad Request";
const std::string HTTP_403 = "403 Forbidden";
const std::string HTTP_404 = "404 Not Found";
const std::string HTTP_405 = "405 Method Not Allowed";
const std::string HTTP_413 = "413 Payload Too Large";
const std::string HTTP_500 = "500 Internal Server Error";

class Request
{
private:
    ServerConfig _config;
    std::vector<ServerConfig> _configs; // Added member variable

    std::string _method;
    std::string _url;
    std::string _http_version;

    std::string _request;
    std::string _response;

    std::string _body;
    ssize_t     _max_body_size;

    std::string _headers;

    bool _needs_redirect = false;
    bool _response_ready = false;

    // Updated method signature
    ServerConfig* selectServerConfig(const std::string &host_header);

public:
    // Updated constructor to initialize _configs
    Request(const std::vector<ServerConfig> &configs, const std::string &request_data);
    Request(const Request &src) = delete;
    Request &operator=(const Request &src) = delete;
    ~Request();

    void ParseRequest();
    void ParseLine(const std::string &line);

    void SendResponse(const std::string &requestBody);
    void GetResponse();
    void PostResponse(const std::string &requestBody);
    void DeleteResponse();

    void executeCGI(std::string path, std::string method, std::string body);
    bool isCgiRequest(std::string path);

    void responseHeader(const std::string &content, const std::string &status_code);
    std::string getStatusMessage(int statuscode);

    ssize_t convertMaxBodySize(const std::string &input);
    void handleFormUrlEncoded(const std::string &requestBody);
    void handlePlainText(const std::string &requestBody);
    void handleJson(const std::string &requestBody);
    void handleMultipartFormData(const std::string &requestBody);
    void handleUnsupportedContentType();
    void sendHtmlResponse(const std::string &htmlContent);

    void ServeErrorPage(int error_code);
    LocationConfig* findLocation(const std::string& url);
    bool isMethodAllowed(LocationConfig* location, const std::string& method);

    std::string generateDirectoryListing(const std::string& directoryPath, const std::string& host, int port);

    void sendRedirectResponse(const std::string &redirection_url, int return_code);

    void createDir(const std::string &path);

    void NormalizeURL();

    bool isResponseReady() const { return _response_ready; }
    std::string getResponse() const { return _response; }
};
