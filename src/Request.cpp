/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   Request.cpp                                        :+:    :+:            */
/*                                                     +:+                    */
/*   By: dvan-kle <dvan-kle@student.codam.nl>         +#+                     */
/*                                                   +#+                      */
/*   Created: 2024/08/01 15:42:12 by dvan-kle      #+#    #+#                 */
/*   Updated: 2024/09/05 17:09:46 by trstn4        ########   odam.nl         */
/*                                                                            */
/* ************************************************************************** */

#include "../include/Request.hpp"
#include <signal.h>
#include <sys/wait.h>

Request::Request(int client_fd) : _client_fd(client_fd)
{
	int bytes_read = read(_client_fd, _buffer, 1024);
	if (bytes_read == -1)
	{
		std::cerr << "Error: read failed" << std::endl;
		close(_client_fd);
		exit(EXIT_FAILURE);
	}
	else if (bytes_read == 0)
	{
		std::cerr << "Error: client disconnected" << std::endl;
		close(_client_fd);
		//exit(EXIT_FAILURE);
	}
	_buffer[bytes_read] = '\0';
}

Request::~Request()
{
	close(_client_fd);
}

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
        char *const envp[] = {NULL}; // Environment variables (idk maybe we need to pass others gotta check)
        char *const cgiArgv[] = {(char *)path.c_str(), NULL}; // Pass the CGI script path as the first argument

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
            dup2(pipeFd[1], STDOUT_FILENO); // Redirect output to pipe
            close(pipeFd[1]);

            // Optionally: we can also set a timeout for the child process itself but idk lets check later
            alarm(2);  // Automatically kill the process after 2 seconds (our timeout method for infin while loop etc)

            // Execute CGI script
            if (path.find(".py") != std::string::npos) {
                char *const pythonArgv[] = {(char *)"python3", (char *)path.c_str(), NULL};
                if (execve("/usr/bin/python3", pythonArgv, envp) == -1) {
                    std::cerr << "Failed to execute Python script" << std::endl;
                    exit(1);
                }
            } else {
                // General CGI script (e.g., Bash or other interpreters)
                if (execve(path.c_str(), cgiArgv, envp) == -1) {
                    std::cerr << "Failed to execute CGI script" << std::endl;
                    exit(1);
                }
            }
        } else {
            // Parent process: Read CGI output
            close(pipeFd[1]);

            // Implement timeout: Set the timeout for 2 seconds
            time_t startTime = time(nullptr);
            int status;
            pid_t result;

            while (true) {
                result = waitpid(pid, &status, WNOHANG);
                if (result == 0) {
                    // The child process is still running; check the timeout
                    if (time(nullptr) - startTime >= 2) {
                        kill(pid, SIGKILL); // Kill the process if it exceeds 2 seconds
                        waitpid(pid, &status, 0); // Wait for the process to be killed
                        std::cerr << "CGI script execution timed out" << std::endl;
                        throw std::runtime_error("CGI script timeout");
                    }
                } else if (result == -1) {
                    std::cerr << "Failed to wait for CGI process" << std::endl;
                    throw std::runtime_error("Internal Server Error");
                } else {
                    // The child process has exited
                    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                        std::cerr << "CGI script exited with status " << WEXITSTATUS(status) << std::endl;
                        throw std::runtime_error("CGI script failed");
                    }
                    break;
                }
                // Allow for some delay before rechecking
                usleep(10000); // Sleep for 10ms before checking again
            }

            // Read CGI output from the pipe and send it to the client
            char buffer[1024];
            std::string response;
            int bytesRead;
            while ((bytesRead = read(pipeFd[0], buffer, sizeof(buffer))) > 0) {
                response.append(buffer, bytesRead);
            }
            close(pipeFd[0]);

            // Send the CGI output as the HTTP response
            std::string responseString = _http_version + " 200 OK\r\nContent-Type: text/html\r\n\r\n" + response;
            write(_client_fd, responseString.c_str(), responseString.size());
        }
    } catch (const std::runtime_error &e) {
        // Handle the timeout or any other CGI error
        std::cerr << "Error: " << e.what() << std::endl;

        // Send a 504 Gateway Timeout response to the client
        std::string errorResponse = _http_version + " 504 Gateway Timeout\r\nContent-Type: text/html\r\n\r\n";
        errorResponse += "<html><body><h1>504 Gateway Timeout</h1><p>The CGI script timed out.</p></body></html>";
        write(_client_fd, errorResponse.c_str(), errorResponse.size());
    }
}

void Request::ParseRequest() {
    std::cout << _buffer << std::endl;
    std::string request(_buffer);
    std::string::size_type pos = request.find("\r\n");

    if (pos != std::string::npos) {
        std::string requestLine = request.substr(0, pos);
        ParseLine(requestLine);

        std::cout << "Method: " << _method << std::endl;
        std::cout << "URL: " << _url << std::endl;
        std::cout << "HTTP Version: " << _http_version << std::endl << std::endl;

        if (isCgiRequest(_url)) {
            executeCGI(WWW_FOLD + _url, _method, request);
        } else {
            SendResponse(request);
        }
    } else {
        std::cerr << "Invalid HTTP request" << std::endl;
    }
}

void Request::ParseLine(std::string line)
{
	std::string::size_type pos = 0;
	std::string::size_type prev = 0;
	
	pos = line.find(" ");
	_method = line.substr(prev, pos);
	prev = pos + 1;
	
	pos = line.find(" ", prev);
	_url = line.substr(prev, pos - prev);
	prev = pos + 1;
	
	_http_version = line.substr(prev);
}

void Request::SendResponse(std::string request)
{
	if (_method == "GET")
		GetResponse();
	else if (_method == "POST")
		PostResponse(request);
	else
		MethodNotAllowed();
		
}

void Request::GetResponse()
{
	if (_url == "/")
		_url = "/index.html";
	std::string responsefile  = WWW_FOLD + _url;
	std::cout << responsefile << std::endl;
	
	std::ifstream ifstr(responsefile, std::ios::binary);
	if (!ifstr)
		return (PageNotFound());
	std::string htmlContent((std::istreambuf_iterator<char>(ifstr)), std::istreambuf_iterator<char>());
	_response += _http_version + " " + HTTP_200;
	if (_url.find(".css") != std::string::npos)
		_response += CON_TYPE_CSS;
	else
		_response += CONTYPE_HTML;
	_response += CONTENT_LENGTH + std::to_string(htmlContent.size()) + "\r\n\r\n";
	_response += htmlContent;
	

	ssize_t bytes_written = write(_client_fd, _response.c_str(), strlen(_response.c_str()));
	if (bytes_written == -1)
	{
		std::cerr << "Error: write failed" << std::endl;
		close(_client_fd);
		exit(EXIT_FAILURE);
	}
	//std::cout << _response << std::endl;
}
