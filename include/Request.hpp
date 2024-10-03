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
const std::string HTTP_415 = "415 Unsupported Media Type";
const std::string HTTP_500 = "500 Internal Server Error";

class Request
{
    private:
        ServerConfig				_config;
        std::vector<ServerConfig>	_configs; // Added member variable

        std::string					_method;
        std::string					_url;
        std::string					_http_version;

        std::string					_request;
        std::string					_response;

        std::string					_body;
        ssize_t    					_max_body_size;

        std::string					_headers;

        int							_port;

        bool _needs_redirect = false;
        bool _response_ready = false;

        // Updated method signature
        ServerConfig* selectServerConfig(const std::string &host_header);

        // Header Parsing and Request Handling
        void ParseHeadersAndBody(); // Split headers and body parsing logic
        std::string ExtractRequestLine(); // Extract the request line from headers
        void HandleRequest(); // Handle request after selecting the config

        // URL Parsing and Normalization
        void ParseLine(const std::string &line);
        void NormalizeURL();

        // Response Handling
        void HandleGetRequest(); // Handle GET requests
        void HandlePostRequest(const std::string &requestBody);
        void HandleDeleteRequest(); // Handle DELETE requests

        // File and Directory Handling
        void ServeFileOrDirectory(const std::string &filePath, LocationConfig* location); // Handle file or directory requests
        void HandleDirectoryRequest(const std::string &filePath, LocationConfig* location); // Handle directory requests
        void ServeFile(const std::string &filePath); // Serve a file to the client

        // Newly added private methods for handling CGI execution
        LocationConfig* validateCgiRequest(std::string& path); // Validate CGI request and prepare environment
        bool setupPipes(int stdinPipe[2], int stdoutPipe[2], int stderrPipe[2]);  // Setup pipes for communication
        void handleCgiChildProcess(int stdinPipe[2], int stdoutPipe[2], int stderrPipe[2], LocationConfig* location, const std::string& scriptPath, const std::string& method, const std::string& body);  // Handle child process logic
        bool executeCgiScript(LocationConfig* location, const std::string& scriptPath, char* const envp[]);  // Execute CGI script
        void handleCgiParentProcess(int stdinPipe[2], int stdoutPipe[2], int stderrPipe[2], const std::string& body, pid_t pid);  // Handle parent process logic
        void writeBodyToPipe(const std::string& body, int writeFd);  // Write the request body to the CGI script via stdin pipe
        bool monitorCgiExecution(pid_t pid, int stdoutPipe, int stderrPipe);  // Monitor CGI process and handle timeouts/errors
        void processCgiOutput(int stdoutPipe, int stderrPipe);  // Process CGI script output and prepare the HTTP response

    public:
        // Updated constructor to initialize _configs
        Request(const std::vector<ServerConfig> &configs, const std::string &request_data, int port);
        Request(const Request &src) = delete;
        Request &operator=(const Request &src) = delete;
        ~Request();

        // Main Request Parsing and Execution
        void ParseRequest(); 

        // CGI and Method Utilities
        void executeCGI(std::string path, std::string method, std::string body);
        bool isCgiRequest(std::string path);

        // Response Utilities
        void responseHeader(const std::string &content, const std::string &status_code);
        void ServeErrorPage(int error_code);

        // Additional Helpers for POST, DELETE, and Response Handling
        void DeleteResponse();
        void handleFormUrlEncoded(const std::string &requestBody);
        void handlePlainText(const std::string &requestBody);
        void handleJson(const std::string &requestBody);
        void handleMultipartFormData(const std::string &requestBody);
        void handleUnsupportedContentType();
        void sendHtmlResponse(const std::string &htmlContent);

        // Directory Listing and Auto-Indexing
        void ServeAutoIndex(const std::string& directoryPath, const std::string& url, const std::string& host, int port, LocationConfig* location);

        // URL Redirection
        void sendRedirectResponse(const std::string &redirection_url, int return_code);

        // Utilities
        std::string getCurrentTimeHttpFormat();
        bool hasFileExtension(const std::string& url);
        void createDir(const std::string &path);

        // Location and Method Utilities
        LocationConfig* findLocation(const std::string& url);
        bool isMethodAllowed(LocationConfig* location, const std::string& method);

        // Response Readiness
        bool isResponseReady() const { return _response_ready; }
        std::string getResponse() const { return _response; }

        std::string getAbsolutePath(const std::string &path);

    // Convert max body size from configuration string (e.g., "1M", "1K") to bytes.
    ssize_t convertMaxBodySize(const std::string &input);

    // Handle POST request
    // Extract Content-Length from headers
    size_t extractContentLength();

    // Extract Content-Type from headers and normalize it
    std::string extractContentType();

    // Process request body based on content type
    void processRequestBody(const std::string &contentType, const std::string &requestBody);

    // Extract multipart boundary from headers
    std::string extractBoundary();

    // Process multipart form-data and save files
    void processMultipartData(const std::string &requestBody, const std::string &boundary, const std::string &uploadDir);

    // Extract filename from multipart headers
    std::string extractFilename(const std::string &partHeaders);

    // Save uploaded file to specified path
    void saveUploadedFile(const std::string &uploadDir, const std::string &filename, const std::string &fileContent);
};
