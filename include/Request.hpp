#pragma once

#include "JsonParser.hpp"
#include "Libaries.hpp"
#include "JsonParser.hpp"

const std::string HTTP_200 = "200 OK";
const std::string HTTP_400 = "400 Bad Request";
const std::string HTTP_403 = "403 Forbidden";
const std::string HTTP_404 = "404 Not Found";
const std::string HTTP_405 = "405 Method Not Allowed";
const std::string HTTP_413 = "413 Payload Too Large";
const std::string HTTP_500 = "500 Internal Server Error";
const std::string CONTYPE_HTML = "Content-Type: text/html\r\n";
const std::string CONTYPE_CSS = "Content-Type: text/css\r\n";
const std::string CONTENT_LENGTH = "Content-Length: ";

class Request
{
private:
    int _client_fd;
    char _buffer[1024];
    
    std::string _method;
    std::string _url;
    std::string _http_version;
    
    std::string _request;
    std::string _response;

    std::string _body;
    ssize_t     _max_body_size;

    std::string _headers; // Add this line

    ServerConfig _config;

public:
    Request(int client_fd, ServerConfig server);
    Request(const Request &src) = delete;
    Request &operator=(const Request &src) = delete;
    ~Request();

    void ParseRequest();
    void ParseLine(std::string line);

    void SendResponse(const std::string &requestBody); // Adjusted parameter type
    void GetResponse();
    void PostResponse(const std::string &requestBody); // Adjusted parameter type
    void DeleteResponse();
    
    void createDir(const std::string &name); // Adjusted parameter type

    void executeCGI(std::string path, std::string method, std::string body);
    bool isCgiRequest(std::string path);
    
    void responseHeader(const std::string &htmlContent, const std::string &status_code);
    std::string unchunkRequestBody(const std::string &buffer);
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
};

    std::string getCurrentTimeHttpFormat();