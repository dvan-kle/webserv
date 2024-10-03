#include "../include/Request.hpp"
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <sys/epoll.h>

// check if the request is for a CGI script based on the file extension
bool Request::isCgiRequest(std::string path) {
    // retrieve the location configuration for the current URL
    LocationConfig* location = findLocation(_url);

    // check if the location is valid and has specified CGI extensions
    if (location != nullptr && !location->cgi_extension.empty()) {
        // find the position of a query string (if any) and remove it from the path
        std::string::size_type queryPos = path.find("?");
        if (queryPos != std::string::npos) {
            // remove query string if present
            path = path.substr(0, queryPos);  
        }

        // find the last occurrence of a '.' to identify the file extension
        std::string::size_type dotPos = path.find_last_of('.');
        if (dotPos != std::string::npos) {
            // extract the file extension
            std::string ext = path.substr(dotPos);

            // check if the file extension matches any of the configured CGI extensions
            for (const auto& valid_ext : location->cgi_extension) {
                if (ext == valid_ext) {
                    return true;  // CGI request detected based on file extension
                }
            }

            // if the extension is not supported, return a 415 error (Unsupported Media Type)
            std::cerr << "Unsupported extension: " << ext << std::endl;
            ServeErrorPage(415);
            return false;
        }
    }

    // if no valid CGI extension is found, return false (not a CGI request)
    return false;
}

// execute the CGI script. forks a new process to run the CGI script and manages input/output via pipes.
void Request::executeCGI(std::string path, std::string method, std::string body) {
    try {
        // validate the CGI request and prepare the environment for execution
        // checks if the path and location are valid for CGI execution.
        LocationConfig* location = validateCgiRequest(path);
        if (location == nullptr) {
            // if the location is invalid, return
            return;
        }

        // prepare pipes for communication between the parent and child processes
        // stdinPipe: for sending input to the CGI script (e.g., POST data)
        // stdoutPipe: for capturing the output of the CGI script
        // stderrPipe: for capturing any errors from the CGI script
        int stdinPipe[2], stdoutPipe[2], stderrPipe[2];
        if (!setupPipes(stdinPipe, stdoutPipe, stderrPipe)) {
            // if pipe setup fails, return
            return;  
        }

        // fork the process to create a child process that will execute the CGI script
        pid_t pid = fork();
        if (pid == -1) {
            // if forking fails, log the error and return a 500 Internal Server Error
            std::cerr << "Failed to fork" << std::endl;
            ServeErrorPage(500);
            return;
        }

        if (pid == 0) {
            // child process: responsible for executing the CGI script
            // this process will handle reading input, executing the script, and writing output.
            handleCgiChildProcess(stdinPipe, stdoutPipe, stderrPipe, location, path, method, body);
        } else {
            // parent process: responsible for managing input/output with the CGI script
            // this process writes any input (like POST data) to the CGI script and captures its output and errors.
            handleCgiParentProcess(stdinPipe, stdoutPipe, stderrPipe, body, pid);
        }
    } catch (const std::runtime_error &e) {
        // if any runtime error occurs during CGI execution, log the error and send a 500 Internal Server Error
        std::cerr << "CGI runtime error: " << e.what() << std::endl;
        ServeErrorPage(500);
    }
}

// this function ensures that the CGI request is valid and prepares the necessary environment for execution.
LocationConfig* Request::validateCgiRequest(std::string& path) {
    // find the location configuration for the given URL (_url).
    LocationConfig* location = findLocation(_url);

    // if no location is found or the root path for the location is empty, log an error and serve a 404 page.
    if (location == nullptr || location->root.empty()) {
        std::cerr << "CGI request failed: No valid location or root defined for " << _url << std::endl;
        ServeErrorPage(404);
        // return null to indicate failure
        return nullptr;
    }

    // check if the URL contains a query string (indicated by '?').
    // if a query string is present, it is removed from the path for CGI execution.
    size_t queryPos = path.find("?");
    if (queryPos != std::string::npos) {
        // remove the query string portion from the path
        path = path.substr(0, queryPos);
    }

    // remove the location path from the URL if it is part of the path.
    // this step ensures that the script path is relative to the root directory of the location.
    if (path.find(location->path) == 0) {
        path = path.substr(location->path.length());
    }

    // remove any leading slashes from the path to standardize the format.
    // this makes the path relative to the location root directory.
    if (!path.empty() && path[0] == '/') {
        // remove the leading slash
        path = path.substr(1);
    }

    // the function returns the validated location object, but no need to prepend the root directory to the path here.
    // the root directory is handled later in the child process.
    return location;
}

// this function is executed by the child process after the parent forks. It handles the actual CGI script execution.
void Request::handleCgiChildProcess(int stdinPipe[2], int stdoutPipe[2], int stderrPipe[2], LocationConfig* location, const std::string& scriptPath, const std::string& method, const std::string& body) {
    // close the unused ends of the pipes in the child process.
    close(stdinPipe[1]); // close the write end of the stdin pipe
    close(stdoutPipe[0]); // close the read end of the stdout pipe
    close(stderrPipe[0]); // close the read end of the stderr pipe

    // redirect the pipes to the standard input/output/error of the child process.
    // this allows the CGI script to read from the stdinPipe and write to the stdoutPipe and stderrPipe.
    dup2(stdinPipe[0], STDIN_FILENO); // redirect stdin to the read end of the pipe
    dup2(stdoutPipe[1], STDOUT_FILENO); // redirect stdout to the write end of the pipe
    dup2(stderrPipe[1], STDERR_FILENO); // redirect stderr to the write end of the pipe

    // close the original pipe file descriptors after duplicating them.
    close(stdinPipe[0]); // close the original read end of the stdin pipe
    close(stdoutPipe[1]); // close the original write end of the stdout pipe
    close(stderrPipe[1]); // close the original write end of the stderr pipe

    // change the current working directory to the root directory of the location.
    // this ensures that the script is executed relative to the correct directory.
    if (chdir(location->root.c_str()) != 0) {
        // if changing the directory fails, log an error to stderr and exit the child process.
        write(STDERR_FILENO, "Failed to change directory to root\n", 35);
        // exit the child process with an error status
        _exit(1);
    }

    // prepare environment variables for the CGI script.
    // these include the request method, content length, script name, query string, and content type.
    std::vector<std::string> env_vars = {
        "REQUEST_METHOD=" + method,                               // HTTP request method (e.g., GET, POST)
        "CONTENT_LENGTH=" + std::to_string(body.length()),        // length of the body (used in POST requests)
        "SCRIPT_NAME=" + scriptPath,                              // the path to the script
        "QUERY_STRING=" + (_url.find("?") != std::string::npos ? _url.substr(_url.find("?") + 1) : ""),  // query string if present
        "CONTENT_TYPE=application/x-www-form-urlencoded"          // content type for form submissions
    };

    // convert the environment variables to the format required by execve (array of char pointers).
    std::vector<char*> envp;
    for (size_t i = 0; i < env_vars.size(); ++i) {
        // convert std::string to char*
        envp.push_back(const_cast<char*>(env_vars[i].c_str()));
    }
    // null-terminate the array
    envp.push_back(nullptr);

    // execute the CGI script using the relative script path.
    if (!executeCgiScript(location, scriptPath, envp.data())) {
        // if execution fails, log an error to stderr and exit the child process with an error status.
        write(STDERR_FILENO, "CGI execution failed\n", 21);
        // exit the child process with an error status
        _exit(1);
    }
}

// setup pipes for communication between parent and child process (including stderr)
// this function creates three pipes: one for stdin, one for stdout, and one for stderr
bool Request::setupPipes(int stdinPipe[2], int stdoutPipe[2], int stderrPipe[2]) {
    // create the stdin pipe. If it fails, log the error and return false
    if (pipe(stdinPipe) == -1) {
        std::cerr << "Failed to create stdin pipe: " << strerror(errno) << std::endl;
        ServeErrorPage(500);
        // indicate failure
        return false;
    }

    // create the stdout pipe. If it fails, log the error and return false
    if (pipe(stdoutPipe) == -1) {
        std::cerr << "Failed to create stdout pipe: " << strerror(errno) << std::endl;
        ServeErrorPage(500);
        // indicate failure
        return false;
    }

    // Create the stderr pipe. If it fails, log the error and return false
    if (pipe(stderrPipe) == -1) {
        std::cerr << "Failed to create stderr pipe: " << strerror(errno) << std::endl;
        ServeErrorPage(500);
        // indicate failure
        return false;
    }

    // if all pipes are successfully created, return true
    return true;
}

// Execute the CGI script using 'execve' based on the file extension and configured CGI path
bool Request::executeCgiScript(LocationConfig* location, const std::string& scriptPath, char* const envp[]) {
    // use the full script path (relative to the location root)
    std::string scriptName = scriptPath;

    // find the last occurrence of a dot ('.') to determine the file extension
    std::string::size_type dotPos = scriptName.find_last_of('.');
    if (dotPos == std::string::npos) {
        // if no file extension is found, log an error and return false
        write(STDERR_FILENO, "Failed to determine file extension\n", 35);
        return false;
    }

    // extract the file extension from the script name
    std::string ext = scriptName.substr(dotPos);

    // iterate over the configured CGI extensions in the location
    for (size_t i = 0; i < location->cgi_extension.size(); ++i) {
        // if the script extension matches a valid CGI extension
        if (ext == location->cgi_extension[i]) {
            // prepare the arguments for execve: CGI interpreter and script path
            char *const cgiExecArgv[] = {
                (char *)location->cgi_path[i].c_str(), // CGI interpreter (e.g., /usr/bin/python3)
                (char *)scriptName.c_str(), // path to the script being executed
                NULL // null-terminate the argument list
            };

            // execute the CGI script using execve with the CGI interpreter and environment variables
            execve(location->cgi_path[i].c_str(), cgiExecArgv, envp);

            // if execve fails, log the error message (using strerror to get system error description)
            write(STDERR_FILENO, "Failed to execute CGI script: ", 30);
            write(STDERR_FILENO, strerror(errno), strlen(strerror(errno)));
            write(STDERR_FILENO, "\n", 1);

            // return false as execve failed
            return false;
        }
    }

    // if no valid interpreter, return false
    write(STDERR_FILENO, "No valid CGI interpreter found\n", 31);
    return false;
}

void Request::handleCgiParentProcess(int stdinPipe[2], int stdoutPipe[2], int stderrPipe[2], const std::string& body, pid_t pid) {
    // Close the unused ends of the pipes
    close(stdinPipe[0]);
    close(stdoutPipe[1]);
    close(stderrPipe[1]);

    // Write the request body to the CGI script's stdin
    writeBodyToPipe(body, stdinPipe[1]);
    close(stdinPipe[1]);  // Close stdin after writing the body

    // Add stdout and stderr pipes to the epoll instance for monitoring
    AddPipeToEpoll(stdoutPipe[0], EPOLLIN);
    AddPipeToEpoll(stderrPipe[0], EPOLLIN);

    // Store CGI execution details (pid, pipes) for later use
    _cgi_pipes[stdoutPipe[0]] = {pid, CGI_STDOUT};
    _cgi_pipes[stderrPipe[0]] = {pid, CGI_STDERR};
}

// Helper function to add a pipe to the epoll instance
void Request::AddPipeToEpoll(int pipe_fd, uint32_t events) {
    _event.events = events | EPOLLET;  // Use edge-triggered mode
    _event.data.fd = pipe_fd;
    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, pipe_fd, &_event) == -1) {
        std::cerr << "Failed to add pipe to epoll: " << strerror(errno) << std::endl;
        close(pipe_fd);
    }
}



// write the request body to the CGI script via the stdin pipe
void Request::writeBodyToPipe(const std::string& body, int writeFd) {
    // check if there is any body content to write
    if (!body.empty()) {
        ssize_t totalWritten = 0; // tracks how much of the body has been written
        ssize_t bytesToWrite = body.length(); // the total number of bytes to write
        const char* bodyData = body.c_str(); // pointer to the body content

        // continue writing until all bytes have been written
        while (totalWritten < bytesToWrite) {
            // write a portion of the body to the write end of the pipe
            ssize_t bytesWritten = write(writeFd, bodyData + totalWritten, bytesToWrite - totalWritten);
            
            // check if the write operation failed
            if (bytesWritten == -1) {
                std::cerr << "failed to write to stdin pipe" << std::endl;
                ServeErrorPage(500);
                // close the write end of the pipe
                close(writeFd);
                return;
            }

            // update the number of bytes written so far
            totalWritten += bytesWritten;
        }
    }
}

// monitor the CGI process execution and handle timeouts or errors
bool Request::monitorCgiExecution(pid_t pid, int stdoutPipe, int stderrPipe) {
    int status; // stores the status of the child process
    pid_t result;
    const int timeout_seconds = 3; // set timeout for CGI execution (seconds)
    time_t start_time = time(NULL); // record the start time

    // loop to monitor the CGI process
    while (true) {
        result = waitpid(pid, &status, WNOHANG);  // non-blocking wait for the child process
        if (result == 0) {
            // check if the timeout has been exceeded
            if (time(NULL) - start_time >= timeout_seconds) {
                // kill the process if it exceeds the timeout
                kill(pid, SIGKILL); // forcefully terminate the child process
                waitpid(pid, &status, 0);  // wait for the process to terminate

                std::cerr << "CGI script execution timed out" << std::endl;

                ServeErrorPage(504); // serve a "Gateway Timeout" error

                close(stdoutPipe); // close the stdout pipe
                close(stderrPipe); // close the stderr pipe

                return false;  // timeout occurred, return failure
            }
            usleep(10000); // sleep for 10ms before checking the process again
        } else if (result == -1) {
            // error occurred while waiting for the child process
            std::cerr << "Failed to wait for CGI process" << std::endl;
            ServeErrorPage(500);
            close(stdoutPipe); // close the stdout pipe
            close(stderrPipe); // close the stderr pipe
            return false; // return failure due to waitpid error
        } else {
            // the child process has finished
            break;
        }
    }

    // check if the child process exited with an error status
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        // log the error exit status
        std::cerr << "CGI script exited with status " << WEXITSTATUS(status) << std::endl;
        // capture and process the CGI output (both stdout and stderr)
        processCgiOutput(stdoutPipe, stderrPipe);
        return false; // return failure due to non-zero exit status
    }

    // if the process finished successfully, process the CGI output and errors
    processCgiOutput(stdoutPipe, stderrPipe);
    return true; // return success
}

// process the output from the CGI script and prepare the HTTP response
void Request::processCgiOutput(int stdoutPipe, int stderrPipe) {
    // buffer to hold data read from pipes
    char buffer[1024];

    std::string cgiOutput; // string to accumulate the standard output from the CGI script
    std::string cgiErrors; // string to accumulate any errors (stderr) from the CGI script
    ssize_t bytesRead;

    // read from stdout (normal CGI output)
    while ((bytesRead = read(stdoutPipe, buffer, sizeof(buffer))) > 0) {
        cgiOutput.append(buffer, bytesRead); // append the data read from stdout to the cgiOutput string
    }

    // read from stderr (errors)
    while ((bytesRead = read(stderrPipe, buffer, sizeof(buffer))) > 0) {
        cgiErrors.append(buffer, bytesRead); // append the data read from stderr to the cgiErrors string
    }

    // prepare the full HTTP response body
    std::string responseBody = "<html><body>";
    // add CGI output to the response
    responseBody += "<h1>CGI Output</h1><pre>" + cgiOutput + "</pre>";

    if (!cgiErrors.empty()) {
        // if there are errors, append the stderr output to the response to make the errors visible
        responseBody += "<h2>CGI Errors</h2><pre>" + cgiErrors + "</pre>";
    }

    // close the HTML body
    responseBody += "</body></html>";

    // prepare the HTTP response with headers
    _response = _http_version + " 200 OK\r\n";
    _response += "Content-Type: text/html\r\n"; // set content type as HTML
    _response += "Content-Length: " + std::to_string(responseBody.length()) + "\r\n"; // set content length header
    _response += "\r\n"; // end of headers
    _response += responseBody; // append the body of the HTTP response

    // log CGI output and errors for debugging purposes
    std::cout << "CGI Output: " << cgiOutput << std::endl; // log the CGI output
    if (!cgiErrors.empty()) {
        std::cerr << "CGI Errors: " << cgiErrors << std::endl; // log any errors from stderr
    }
}
