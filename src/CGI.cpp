#include "../include/Request.hpp"
#include "../include/WriteClient.hpp"

// Check if request is a CGI request
bool Request::isCgiRequest(std::string path) {
    std::string::size_type dotPos = path.find_last_of('.');
    if (dotPos != std::string::npos) {
        std::string ext = path.substr(dotPos);
        return (ext == ".cgi" || ext == ".py");
    }
    return false;
}

void Request::executeCGI(std::string path, std::string method, std::string body) {
    try {
        // Set up environment variables for CGI
        std::string contentLength = std::to_string(body.length());
        std::string requestMethod = method;
        std::string scriptName = path;
        std::string queryString = "";

        std::cout << path << "\n\n";

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

        char *const cgiArgv[] = {(char *)path.c_str(), NULL};

        int pipeFd[2];
        if (pipe(pipeFd) == -1) {
            std::cerr << "Failed to create pipe" << std::endl;
            throw std::runtime_error("Internal Server Error");
        }

        pid_t pid = fork();
        if (pid == -1) {
            std::cerr << "Failed to fork" << std::endl;
            throw std::runtime_error("Internal Server Error");
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

            alarm(2);  // Kill process if it exceeds 2 seconds

            if (path.find(".py") != std::string::npos) {
                char *const pythonArgv[] = {(char *)"python3", (char *)path.c_str(), NULL};
                if (execve("/usr/bin/python3", pythonArgv, envp) == -1) {
                    std::cerr << "Failed to execute Python script" << std::endl;
                    exit(1);
                }
            } else {
                if (execve(path.c_str(), cgiArgv, envp) == -1) {
                    std::cerr << "Failed to execute CGI script" << std::endl;
                    exit(1);
                }
            }
        } else {
            close(pipeFd[1]);

            // Wait for CGI execution and handle timeouts
            time_t startTime = time(nullptr);
            int status;
            pid_t result;

            while (true) {
                result = waitpid(pid, &status, WNOHANG);
                if (result == 0 && time(nullptr) - startTime >= 2) {
                    kill(pid, SIGKILL);  // Kill the process if it exceeds 2 seconds
                    waitpid(pid, &status, 0);
                    std::cerr << "CGI script execution timed out" << std::endl;
                    throw std::runtime_error("CGI script timeout");
                } else if (result == -1) {
                    std::cerr << "Failed to wait for CGI process" << std::endl;
                    throw std::runtime_error("Internal Server Error");
                } else if (result > 0) {
                    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                        std::cerr << "CGI script exited with status " << WEXITSTATUS(status) << std::endl;
                        throw std::runtime_error("CGI script failed");
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
        std::cerr << "Error: " << e.what() << std::endl;
        std::string errorResponse = _http_version + " 504 Gateway Timeout\r\nContent-Type: text/html\r\n\r\n";
        errorResponse += "<html><body><h1>504 Gateway Timeout</h1><p>The CGI script timed out.</p></body></html>";
        write(_client_fd, errorResponse.c_str(), errorResponse.size());
    }
}
