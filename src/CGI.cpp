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
            ServeErrorPage(404);
            return;
        }

        size_t queryPos = path.find("?");
        std::string queryString = "";
        if (queryPos != std::string::npos) {
            queryString = path.substr(queryPos + 1);
            path = path.substr(0, queryPos);  // Remove query string
        }

        if (path.find(location->path) == 0) {
            path = path.substr(location->path.length());  // Remove the location path part
        }
        if (!path.empty() && path[0] == '/') {
            path = path.substr(1);
        }

        std::string scriptPath = location->root + "/" + path;
        std::string scriptDirectory = scriptPath.substr(0, scriptPath.find_last_of('/'));
        std::string scriptName = scriptPath.substr(scriptPath.find_last_of('/') + 1);

        std::string contentLength = std::to_string(body.length());
        std::string requestMethod = method;

        std::string contentType = "application/x-www-form-urlencoded";

        // Prepare environment variables
        std::vector<std::string> env_vars = {
            "REQUEST_METHOD=" + requestMethod,
            "CONTENT_LENGTH=" + contentLength,
            "SCRIPT_NAME=" + scriptName,
            "QUERY_STRING=" + queryString,
            "CONTENT_TYPE=" + contentType,
            "PATH_INFO=" + scriptPath
        };

        // Convert std::string to char*
        std::vector<char*> envp;
        for (size_t i = 0; i < env_vars.size(); ++i) {
            envp.push_back(const_cast<char*>(env_vars[i].c_str()));
        }
        envp.push_back(NULL); // Null-terminate the array

        int stdinPipe[2];
        int stdoutPipe[2];
        if (pipe(stdinPipe) == -1 || pipe(stdoutPipe) == -1) {
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
            // Child process
            close(stdinPipe[1]);   // Close write end of stdin pipe
            close(stdoutPipe[0]);  // Close read end of stdout pipe

            dup2(stdinPipe[0], STDIN_FILENO);
            dup2(stdoutPipe[1], STDOUT_FILENO);

            close(stdinPipe[0]);
            close(stdoutPipe[1]);

            if (chdir(scriptDirectory.c_str()) != 0) {
                std::cerr << "Failed to change directory to " << scriptDirectory << std::endl;
                exit(1);
            }

            // Find CGI interpreter
            if (!location->cgi_path.empty() && !location->cgi_extension.empty()) {
                std::string::size_type dotPos = scriptName.find_last_of('.');
                if (dotPos != std::string::npos) {
                    std::string ext = scriptName.substr(dotPos);

                    for (size_t i = 0; i < location->cgi_extension.size(); ++i) {
                        if (ext == location->cgi_extension[i]) {
                            char *const cgiExecArgv[] = {
                                (char *)location->cgi_path[i].c_str(),
                                (char *)scriptName.c_str(),  // Use scriptName instead of scriptPath
                                NULL
                            };

                            execve(location->cgi_path[i].c_str(), cgiExecArgv, envp.data());

                            std::cerr << "Failed to execute CGI script" << std::endl;
                            exit(1);
                        }
                    }

                    std::cerr << "No valid CGI interpreter found for extension: " << ext << std::endl;
                    exit(1);
                } else {
                    std::cerr << "Failed to determine file extension for " << scriptName << std::endl;
                    exit(1);
                }
            } else {
                std::cerr << "No valid CGI path or extension configured" << std::endl;
                exit(1);
            }
        } else {
            // Parent process
            close(stdinPipe[0]);   // Close read end of stdin pipe
            close(stdoutPipe[1]);  // Close write end of stdout pipe

            if (!body.empty()) {
                ssize_t totalWritten = 0;
                ssize_t bytesToWrite = body.length();
                const char* bodyData = body.c_str();

                while (totalWritten < bytesToWrite) {
                    ssize_t bytesWritten = write(stdinPipe[1], bodyData + totalWritten, bytesToWrite - totalWritten);
                    if (bytesWritten == -1) {
                        std::cerr << "Failed to write to stdin pipe" << std::endl;
                        ServeErrorPage(500);
                        close(stdinPipe[1]);
                        close(stdoutPipe[0]);
                        return;
                    }
                    totalWritten += bytesWritten;
                }
            }

            close(stdinPipe[1]);  // Signal EOF to child process

            // Wait for child process
            int status;
            pid_t result = waitpid(pid, &status, 0);
            if (result == -1) {
                std::cerr << "Failed to wait for CGI process" << std::endl;
                ServeErrorPage(500);
                close(stdoutPipe[0]);
                return;
            }

            if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                std::cerr << "CGI script exited with status " << WEXITSTATUS(status) << std::endl;
                ServeErrorPage(500);
                close(stdoutPipe[0]);
                return;
            }

            // Read CGI output
            char buffer[1024];
            std::string cgiOutput;
            ssize_t bytesRead;
            while ((bytesRead = read(stdoutPipe[0], buffer, sizeof(buffer))) > 0) {
                cgiOutput.append(buffer, bytesRead);
            }
            close(stdoutPipe[0]);

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
