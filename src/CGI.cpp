#include "../include/Request.hpp"
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <signal.h>

// Check if the request is for a CGI script based on the file extension
bool Request::isCgiRequest(std::string path) {
    LocationConfig* location = findLocation(_url);

    if (location != nullptr && !location->cgi_extension.empty()) {
        std::string::size_type queryPos = path.find("?");
        if (queryPos != std::string::npos) {
            path = path.substr(0, queryPos);  // Remove query string if present
        }

        std::string::size_type dotPos = path.find_last_of('.');
        if (dotPos != std::string::npos) {
            std::string ext = path.substr(dotPos);
            for (const auto& valid_ext : location->cgi_extension) {
                if (ext == valid_ext) {
                    return true;  // CGI request detected
                }
            }
            std::cerr << "Unsupported extension: " << ext << std::endl;
            ServeErrorPage(415);  // Unsupported Media Type
            return false;
        }
    }

    return false;
}

// Execute the CGI script
void Request::executeCGI(std::string path, std::string method, std::string body) {
    try {
        LocationConfig* location = validateCgiRequest(path);  // Validate request and prepare environment
        if (location == nullptr) return;

        // Prepare pipes for communication (stdin, stdout, stderr)
        int stdinPipe[2], stdoutPipe[2], stderrPipe[2];
        if (!setupPipes(stdinPipe, stdoutPipe, stderrPipe)) return;

        pid_t pid = fork();  // Fork to create child process
        if (pid == -1) {
            std::cerr << "Failed to fork" << std::endl;
            ServeErrorPage(500);
            return;
        }

        if (pid == 0) {
            // Child process: handle the CGI script execution
            handleCgiChildProcess(stdinPipe, stdoutPipe, stderrPipe, location, path, method, body);
        } else {
            // Parent process: handle writing the body (for POST) and reading the CGI output and errors
            handleCgiParentProcess(stdinPipe, stdoutPipe, stderrPipe, body, pid);
        }
    } catch (const std::runtime_error &e) {
        std::cerr << "CGI runtime error: " << e.what() << std::endl;
        ServeErrorPage(500);
    }
}

// Validate CGI request and prepare environment
LocationConfig* Request::validateCgiRequest(std::string& path) {
    LocationConfig* location = findLocation(_url);
    if (location == nullptr || location->root.empty()) {
        std::cerr << "CGI request failed: No valid location or root defined for " << _url << std::endl;
        ServeErrorPage(404);
        return nullptr;
    }

    size_t queryPos = path.find("?");
    if (queryPos != std::string::npos) {
        path = path.substr(0, queryPos);  // Remove query string
    }

    // Remove the location path part if present
    if (path.find(location->path) == 0) {
        path = path.substr(location->path.length());
    }
    if (!path.empty() && path[0] == '/') {
        path = path.substr(1);  // Remove leading slash
    }

    // No need to prepend root here, we will navigate to it in the child process
    return location;
}

// Handle the child process logic for executing the CGI script
void Request::handleCgiChildProcess(int stdinPipe[2], int stdoutPipe[2], int stderrPipe[2], LocationConfig* location, const std::string& scriptPath, const std::string& method, const std::string& body) {
    close(stdinPipe[1]);   // Close write end of stdin pipe
    close(stdoutPipe[0]);  // Close read end of stdout pipe
    close(stderrPipe[0]);  // Close read end of stderr pipe

    dup2(stdinPipe[0], STDIN_FILENO);  // Redirect stdin to pipe
    dup2(stdoutPipe[1], STDOUT_FILENO);  // Redirect stdout to pipe
    dup2(stderrPipe[1], STDERR_FILENO);  // Redirect stderr to pipe

    close(stdinPipe[0]);   // Close original pipe ends after redirecting
    close(stdoutPipe[1]);
    close(stderrPipe[1]);

    // Change to the root directory of the location for correct relative path execution
    if (chdir(location->root.c_str()) != 0) {
        write(STDERR_FILENO, "Failed to change directory to root\n", 35);
        _exit(1);  // Exit child process if directory change fails
    }

    // Prepare environment variables
    std::vector<std::string> env_vars = {
        "REQUEST_METHOD=" + method,
        "CONTENT_LENGTH=" + std::to_string(body.length()),
        "SCRIPT_NAME=" + scriptPath,
        "QUERY_STRING=" + (_url.find("?") != std::string::npos ? _url.substr(_url.find("?") + 1) : ""),
        "CONTENT_TYPE=application/x-www-form-urlencoded"  // Default for form data
    };

    // Convert environment variables to char* format for execve
    std::vector<char*> envp;
    for (size_t i = 0; i < env_vars.size(); ++i) {
        envp.push_back(const_cast<char*>(env_vars[i].c_str()));
    }
    envp.push_back(nullptr);  // Null-terminate the array

    // Execute the CGI script using relative script path
    if (!executeCgiScript(location, scriptPath, envp.data())) {
        write(STDERR_FILENO, "CGI execution failed\n", 21);  // Write to stderr if execve fails
        _exit(1);  // Exit child process if execution fails
    }
}

// Setup pipes for communication between parent and child process (including stderr)
bool Request::setupPipes(int stdinPipe[2], int stdoutPipe[2], int stderrPipe[2]) {
    if (pipe(stdinPipe) == -1 || pipe(stdoutPipe) == -1 || pipe(stderrPipe) == -1) {
        std::cerr << "Failed to create pipes: " << strerror(errno) << std::endl;  // Log system error
        ServeErrorPage(500);
        return false;
    }
    return true;
}

// Execute the CGI script using `execve` based on the extension and configured CGI path
bool Request::executeCgiScript(LocationConfig* location, const std::string& scriptPath, char* const envp[]) {
    std::string scriptName = scriptPath;  // Use the relative script path
    std::string::size_type dotPos = scriptName.find_last_of('.');
    if (dotPos == std::string::npos) {
        write(STDERR_FILENO, "Failed to determine file extension\n", 35);
        return false;
    }

    std::string ext = scriptName.substr(dotPos);
    for (size_t i = 0; i < location->cgi_extension.size(); ++i) {
        if (ext == location->cgi_extension[i]) {
            char *const cgiExecArgv[] = {
                (char *)location->cgi_path[i].c_str(),   // CGI interpreter
                (char *)scriptName.c_str(),              // Use script relative path
                NULL
            };

            execve(location->cgi_path[i].c_str(), cgiExecArgv, envp);  // Pass the environment variables

            // If execve fails, log the error
            write(STDERR_FILENO, "Failed to execute CGI script: ", 30);
            write(STDERR_FILENO, strerror(errno), strlen(strerror(errno)));  // Output system error message
            write(STDERR_FILENO, "\n", 1);
            return false;  // Execve failed
        }
    }

    write(STDERR_FILENO, "No valid CGI interpreter found\n", 31);
    return false;  // No valid interpreter
}


// Handle the parent process logic for managing the CGI process and collecting output and errors
void Request::handleCgiParentProcess(int stdinPipe[2], int stdoutPipe[2], int stderrPipe[2], const std::string& body, pid_t pid) {
    close(stdinPipe[0]);  // Close read end of stdin pipe
    close(stdoutPipe[1]);  // Close write end of stdout pipe
    close(stderrPipe[1]);  // Close write end of stderr pipe

    writeBodyToPipe(body, stdinPipe[1]);

    close(stdinPipe[1]);  // Signal EOF to child process

    // Correct function call: Pass the read end of stdoutPipe and stderrPipe
    if (!monitorCgiExecution(pid, stdoutPipe[0], stderrPipe[0])) {
        return;
    }

    close(stdoutPipe[0]);
    close(stderrPipe[0]);
}

// Write the request body to the CGI script via the stdin pipe
void Request::writeBodyToPipe(const std::string& body, int writeFd) {
    if (!body.empty()) {
        ssize_t totalWritten = 0;
        ssize_t bytesToWrite = body.length();
        const char* bodyData = body.c_str();

        while (totalWritten < bytesToWrite) {
            ssize_t bytesWritten = write(writeFd, bodyData + totalWritten, bytesToWrite - totalWritten);
            if (bytesWritten == -1) {
                std::cerr << "Failed to write to stdin pipe" << std::endl;
                ServeErrorPage(500);
                close(writeFd);
                return;
            }
            totalWritten += bytesWritten;
        }
    }
}

// Monitor the CGI process execution and handle timeouts or errors
bool Request::monitorCgiExecution(pid_t pid, int stdoutPipe, int stderrPipe) {
    int status;
    pid_t result;
    const int timeout_seconds = 3;  // Set timeout for CGI execution
    time_t start_time = time(NULL);

    while (true) {
        result = waitpid(pid, &status, WNOHANG);
        if (result == 0) {
            if (time(NULL) - start_time >= timeout_seconds) {
                // Kill the process if it exceeds the timeout
                kill(pid, SIGKILL);
                waitpid(pid, &status, 0);
                std::cerr << "CGI script execution timed out" << std::endl;
                ServeErrorPage(504);  // Gateway Timeout
                close(stdoutPipe);
                close(stderrPipe);
                return false;
            }
            usleep(10000);  // Sleep for 10ms before checking again
        } else if (result == -1) {
            std::cerr << "Failed to wait for CGI process" << std::endl;
            ServeErrorPage(500);
            close(stdoutPipe);
            close(stderrPipe);
            return false;
        } else {
            break;  // Process finished
        }
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        // Log the error exit status
        std::cerr << "CGI script exited with status " << WEXITSTATUS(status) << std::endl;
        // Capture and process stderr output
        processCgiOutput(stdoutPipe, stderrPipe);
        return false;
    }

    // Process CGI output and errors if exit status is successful
    processCgiOutput(stdoutPipe, stderrPipe);
    return true;
}

// Process the output from the CGI script and prepare the HTTP response
void Request::processCgiOutput(int stdoutPipe, int stderrPipe) {
    char buffer[1024];
    std::string cgiOutput;
    std::string cgiErrors;
    ssize_t bytesRead;

    // Read from stdout (normal CGI output)
    while ((bytesRead = read(stdoutPipe, buffer, sizeof(buffer))) > 0) {
        cgiOutput.append(buffer, bytesRead);
    }

    // Read from stderr (errors)
    while ((bytesRead = read(stderrPipe, buffer, sizeof(buffer))) > 0) {
        cgiErrors.append(buffer, bytesRead);
    }

    // Prepare the full HTTP response
    std::string responseBody = "<html><body>";
    responseBody += "<h1>CGI Output</h1><pre>" + cgiOutput + "</pre>";

    if (!cgiErrors.empty()) {
        // Append the stderr output as part of the response to make the error visible
        responseBody += "<h2>CGI Errors</h2><pre>" + cgiErrors + "</pre>";
    }

    responseBody += "</body></html>";

    _response = _http_version + " 200 OK\r\n";
    _response += "Content-Type: text/html\r\n";
    _response += "Content-Length: " + std::to_string(responseBody.length()) + "\r\n";
    _response += "\r\n";
    _response += responseBody;

    // Log CGI output and errors for debugging purposes
    std::cout << "CGI Output: " << cgiOutput << std::endl;
    if (!cgiErrors.empty()) {
        std::cerr << "CGI Errors: " << cgiErrors << std::endl;
    }
}
