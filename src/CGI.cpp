#include "../include/Request.hpp"
#include "../include/WriteClient.hpp"

// Enhanced function to check for CGI request with appropriate error handling
bool Request::isCgiRequest(std::string path) {
    LocationConfig* location = findLocation(_url);

    if (location != nullptr && !location->cgi_extension.empty()) {
        // Check if the file matches the CGI extension defined in the config
        std::string::size_type dotPos = path.find_last_of('.');
        if (dotPos != std::string::npos) {
            std::string ext = path.substr(dotPos);
            if (ext == location->cgi_extension) {
                return true;
            } else {
                std::cerr << "Unsupported extension: " << ext << std::endl;
                ServeErrorPage(415);
                return false;
            }
        }
    }

    ServeErrorPage(404);
    return false;
}

// Enhanced executeCGI function with detailed error handling
void Request::executeCGI(std::string path, std::string method, std::string body) {
    try {
        // Set up environment variables for CGI
        path = path.substr(1);  // Remove leading '/'
        std::string contentLength = std::to_string(body.length());
        std::string requestMethod = method;
        std::string scriptName = path;
        std::string queryString = "";

        // Handle the query string for GET requests
        size_t queryPos = _url.find("?");
        if (queryPos != std::string::npos) {
            queryString = _url.substr(queryPos + 1);
        }

        char *const envp[] = {
            (char *)("REQUEST_METHOD=" + requestMethod).c_str(),
            (char *)("CONTENT_LENGTH=" + contentLength).c_str(),
            (char *)("SCRIPT_NAME=" + scriptName).c_str(),
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

            // Use the `cgi_path` from the configuration
            LocationConfig* location = findLocation(_url);
            if (location != nullptr && !location->cgi_path.empty()) {
                std::cerr << "Executing CGI script: " << path << " with cgi_path: " << location->cgi_path << std::endl;

                // Execute CGI using the configured cgi_path
                char *const cgiExecArgv[] = {(char *)location->cgi_path.c_str(), (char *)path.c_str(), NULL};

                if (execve(location->cgi_path.c_str(), cgiExecArgv, envp) == -1) {
                    std::cerr << "Failed to execute CGI script using " << location->cgi_path << std::endl;
                    ServeErrorPage(500);
                    exit(1);
                }
            } else {
                std::cerr << "No valid CGI path configured" << std::endl;
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
            std::string responseString = _http_version + " 200 OK\r\nContent-Type: text/html\r\n\r\n" + response;
            WriteClient::safeWriteToClient(_client_fd, responseString);
        }
    } catch (const std::runtime_error &e) {
        std::cerr << "CGI runtime error: " << e.what() << std::endl;
        ServeErrorPage(500);
    }
}
