#pragma once

#include "JsonParser.hpp"
#include "Libaries.hpp"
#include "JsonParser.hpp"

const std::string HTTP_200 = "200 OK\r\n";
const std::string HTTP_400 = "400 Bad Request\r\n";
const std::string HTTP_403 = "403 Forbidden\r\n";
const std::string HTTP_404 = "404 Not Found\r\n";
const std::string HTTP_405 = "405 Method Not Allowed\r\n";
const std::string HTTP_500 = "500 Internal Server Error\r\n";
const std::string CONTYPE_HTML = "Content-Type: text/html\r\n";
const std::string CON_TYPE_CSS = "Content-Type: text/css\r\n";
const std::string CONTENT_LENGTH = "Content-Length: ";
const std::string ERROR_FOLD = "error_pages";
const std::string WWW_FOLD = "www/";

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
    
    void responseHeader(std::string htmlContent, const std::string status_code);
    std::string unchunkRequestBody(const std::string &buffer);
    std::string getCurrentTimeHttpFormat();

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
};
