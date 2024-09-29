#include "../include/Request.hpp"
#include "../include/WriteClient.hpp"

bool Request::isCgiRequest(std::string path) {
    LocationConfig* location = findLocation(_url);

    if (location != nullptr && !location->cgi_extension.empty()) {
        // Find position of '?' to remove query string if present
        std::string::size_type queryPos = path.find("?");
        if (queryPos != std::string::npos) {
            path = path.substr(0, queryPos);  // Remove query string
        }

        // Find the last occurrence of '.' to get the file extension
        std::string::size_type dotPos = path.find_last_of('.');
        if (dotPos != std::string::npos) {
            std::string ext = path.substr(dotPos);
            for (const auto& valid_ext : location->cgi_extension) {
                if (ext == valid_ext) {
                    return true;
                }
            }
            std::cerr << "Unsupported extension: " << ext << std::endl;
            ServeErrorPage(415);  // Unsupported Media Type
            return false;
        }
    }

    return false;
}

void Request::executeCGI(std::string path, std::string method, std::string body) {
    try {
        LocationConfig* location = findLocation(_url);
        if (location == nullptr || location->root.empty()) {
            std::cerr << "CGI request failed: No valid location or root defined for " << _url << std::endl;
            ServeErrorPage(404);  // No valid root for the CGI location
            return;
        }

        // Remove query string from the path (if any)
        size_t queryPos = path.find("?");
        std::string queryString = "";
        if (queryPos != std::string::npos) {
            queryString = path.substr(queryPos + 1);
            path = path.substr(0, queryPos);  // Remove query string
        }

        // Strip the location `path` from the URL to get the script's relative path
        if (path.find(location->path) == 0) {
            path = path.substr(location->path.length());  // Remove the location path part
        }

        // Ensure the path does not have an extra '/' at the beginning
        if (path[0] == '/') {
            path = path.substr(1);  // Remove leading '/'
        }

        // Construct the full script path using the root
        std::string scriptPath = location->root + "/" + path;

        // Set up environment variables for CGI
        std::string contentLength = std::to_string(body.length());
        std::string requestMethod = method;

        // Prepare environment variables
        std::vector<std::string> envVars = {
            "REQUEST_METHOD=" + requestMethod,
            "CONTENT_LENGTH=" + contentLength,
            "SCRIPT_NAME=" + scriptPath,
            "QUERY_STRING=" + queryString,
            "SERVER_PROTOCOL=HTTP/1.1",
            "GATEWAY_INTERFACE=CGI/1.1",
            "SERVER_SOFTWARE=webserv/1.0",
            "SERVER_NAME=" + _config.server_name,
            "SERVER_PORT=" + std::to_string(_config.listen_port),
            "REMOTE_ADDR=127.0.0.1"  // You may want to get the actual client IP
        };

        // Convert environment variables to the format required by execve
        std::vector<char*> envp;
        for (size_t i = 0; i < envVars.size(); ++i) {
            envp.push_back(const_cast<char*>(envVars[i].c_str()));
        }
        envp.push_back(NULL);  // Null-terminate the array

        // Set up pipes for stdin and stdout
        int inputPipeFd[2];   // For passing the request body to the CGI script
        int outputPipeFd[2];  // For reading the CGI script's output

        if (pipe(inputPipeFd) == -1 || pipe(outputPipeFd) == -1) {
            std::cerr << "Failed to create pipes" << std::endl;
            ServeErrorPage(500);
            return;
        }

        pid_t pid = fork();
        if (pid == -1) {
            std::cerr << "Failed to fork" << std::endl;
            ServeErrorPage(500);
            return;
        }

        if (pid == 0) {
            // Child process: Execute the CGI script

            // Redirect stdin
            close(inputPipeFd[1]);           // Close unused write end
            dup2(inputPipeFd[0], STDIN_FILENO);
            close(inputPipeFd[0]);

            // Redirect stdout
            close(outputPipeFd[0]);          // Close unused read end
            dup2(outputPipeFd[1], STDOUT_FILENO);
            close(outputPipeFd[1]);

            // Execute CGI script
            std::string interpreter;
            std::string::size_type dotPos = scriptPath.find_last_of('.');
            if (dotPos != std::string::npos) {
                std::string ext = scriptPath.substr(dotPos);
                for (size_t i = 0; i < location->cgi_extension.size(); ++i) {
                    if (ext == location->cgi_extension[i]) {
                        interpreter = location->cgi_path[i];
                        break;
                    }
                }
            }

            if (interpreter.empty()) {
                std::cerr << "No CGI interpreter found for extension" << std::endl;
                exit(1);
            }

            char* const args[] = {const_cast<char*>(interpreter.c_str()), const_cast<char*>(scriptPath.c_str()), NULL};

            if (execve(interpreter.c_str(), args, envp.data()) == -1) {
                std::cerr << "Failed to execute CGI script" << std::endl;
                exit(1);
            }
        } else {
            // Parent process

            // Close unused pipe ends
            close(inputPipeFd[0]);   // Close read end of input pipe (stdin for CGI)
            close(outputPipeFd[1]);  // Close write end of output pipe (stdout from CGI)

            // Write the body to the CGI's stdin
            if (!body.empty()) {
                write(inputPipeFd[1], body.c_str(), body.length());
            }
            close(inputPipeFd[1]);  // Close write end after writing (EOF for CGI)

            // Read CGI output from stdout
            char buffer[1024];
            std::string cgiOutput;
            int bytesRead;
            while ((bytesRead = read(outputPipeFd[0], buffer, sizeof(buffer))) > 0) {
                cgiOutput.append(buffer, bytesRead);
            }
            close(outputPipeFd[0]);

            // Wait for the child process to finish
            int status;
            waitpid(pid, &status, 0);

            // Separate headers and body from CGI output
            size_t headerEndPos = cgiOutput.find("\r\n\r\n");
            if (headerEndPos != std::string::npos) {
                std::string cgiHeaders = cgiOutput.substr(0, headerEndPos + 4);
                std::string cgiBody = cgiOutput.substr(headerEndPos + 4);

                // Prepare the HTTP response
                _response = _http_version + " 200 OK\r\n" + cgiHeaders + cgiBody;
            } else {
                // If no headers are found, send a default header
                _response = _http_version + " 200 OK\r\n";
                _response += "Content-Type: text/html\r\n";
                _response += "Content-Length: " + std::to_string(cgiOutput.length()) + "\r\n";
                _response += "\r\n";
                _response += cgiOutput;
            }

            // Send the response to the client
            WriteClient::safeWriteToClient(_client_fd, _response);
        }
    } catch (const std::runtime_error &e) {
        std::cerr << "CGI runtime error: " << e.what() << std::endl;
        ServeErrorPage(500);
    }
}

