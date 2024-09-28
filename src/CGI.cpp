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
        if (queryPos != std::string::npos) {
            path = path.substr(0, queryPos);  // Remove query string
        }

        // Strip the location `path` from the URL to get the script's relative path
        if (path.find(location->path) == 0) {
            path = path.substr(location->path.length());  // Remove the `/scripts` part
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
        std::string queryString = "";

        // Handle the query string for GET requests
        if (queryPos != std::string::npos) {
            queryString = _url.substr(queryPos + 1);
        }

        char *const envp[] = {
            (char *)("REQUEST_METHOD=" + requestMethod).c_str(),
            (char *)("CONTENT_LENGTH=" + contentLength).c_str(),
            (char *)("SCRIPT_NAME=" + scriptPath).c_str(),
            (char *)("QUERY_STRING=" + queryString).c_str(),
            NULL
        };

        int pipeFd[2];
        if (pipe(pipeFd) == -1) {
            std::cerr << "Failed to create pipe" << std::endl;
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
            // Child process: Set up to execute CGI
            close(pipeFd[0]);
            dup2(pipeFd[1], STDOUT_FILENO);  // Redirect output to pipe
            close(pipeFd[1]);

            // If the method is POST, write the body to stdin for the CGI script
            if (method == "POST") {
                dup2(STDIN_FILENO, pipeFd[1]);
                write(STDIN_FILENO, body.c_str(), body.length());
            }

            alarm(5);  // Kill process if it exceeds 5 seconds

            // Use the `cgi_path` based on the file extension
            if (!location->cgi_path.empty() && !location->cgi_extension.empty()) {
                // Get the file extension
                std::string::size_type dotPos = scriptPath.find_last_of('.');
                if (dotPos != std::string::npos) {
                    std::string ext = scriptPath.substr(dotPos);

                    // Find the matching CGI interpreter based on the extension
                    bool executed = false;
                    for (size_t i = 0; i < location->cgi_extension.size(); ++i) {
                        if (ext == location->cgi_extension[i]) {
                            std::cerr << "Attempting to execute CGI script: " << scriptPath << " with cgi_path: " << location->cgi_path[i] << std::endl;

                            // Execute CGI using the matched interpreter
                            char *const cgiExecArgv[] = {(char *)location->cgi_path[i].c_str(), (char *)scriptPath.c_str(), NULL};

                            if (execve(location->cgi_path[i].c_str(), cgiExecArgv, envp) != -1) {
                                executed = true;
                                break;
                            }
                        }
                    }

                    if (!executed) {
                        std::cerr << "No valid CGI interpreter found for extension: " << ext << std::endl;
                        ServeErrorPage(500);
                        exit(1);
                    }
                } else {
                    std::cerr << "Failed to determine file extension for " << scriptPath << std::endl;
                    ServeErrorPage(500);
                    exit(1);
                }
            } else {
                std::cerr << "No valid CGI path or extension configured" << std::endl;
                ServeErrorPage(500);
                exit(1);
            }
        } else {
            close(pipeFd[1]);

            // Wait for CGI execution and handle timeouts
            time_t startTime = time(nullptr);
            int status;
            pid_t result;

            while (true) {
                result = waitpid(pid, &status, WNOHANG);
                if (result == 0 && time(nullptr) - startTime >= 5) {  // Timeout handling
                    kill(pid, SIGKILL);  // Kill the process if it exceeds 5 seconds
                    waitpid(pid, &status, 0);
                    std::cerr << "CGI script execution timed out" << std::endl;
                    ServeErrorPage(504);
                    return;
                } else if (result == -1) {
                    std::cerr << "Failed to wait for CGI process" << std::endl;
                    ServeErrorPage(500);
                    return;
                } else if (result > 0) {
                    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                        std::cerr << "CGI script exited with status " << WEXITSTATUS(status) << std::endl;
                        ServeErrorPage(500);
                        return;
                    }
                    break;
                }
                usleep(10000);  // Sleep for 10ms before checking again
            }

            // Read CGI output from the pipe and send it as a response
            char buffer[1024];
            std::string response;
            int bytesRead;
            while ((bytesRead = read(pipeFd[0], buffer, sizeof(buffer))) > 0) {
                response.append(buffer, bytesRead);
            }
            close(pipeFd[0]);

            // Send the CGI output as the HTTP response
            responseHeader(response, HTTP_200);
            WriteClient::safeWriteToClient(_client_fd, _response);
        }
    } catch (const std::runtime_error &e) {
        std::cerr << "CGI runtime error: " << e.what() << std::endl;
        ServeErrorPage(500);
    }
}
