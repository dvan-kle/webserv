#include "../include/Request.hpp"
#include <signal.h>
#include <sys/wait.h>

#include <sstream>

#include "../include/Request.hpp"

Request::Request(int client_fd) : _client_fd(client_fd) {
    std::string headers;
    char buffer[1024];
    int bytes_read;
    size_t total_bytes_read = 0;

    // Read headers
    while (true) {
        bytes_read = read(_client_fd, buffer, sizeof(buffer));
        if (bytes_read == -1) {
            std::cerr << "Error: read failed" << std::endl;
            close(_client_fd);
            exit(EXIT_FAILURE);
        } else if (bytes_read == 0) {
            // Client closed the connection
            break;
        }
        headers.append(buffer, bytes_read);
        total_bytes_read += bytes_read;

        // Check if we have reached the end of headers
        size_t pos = headers.find("\r\n\r\n");
        if (pos != std::string::npos) {
            // We have read all headers
            _headers = headers.substr(0, pos + 4);
            _body = headers.substr(pos + 4); // Any remaining data after headers
            break;
        }
    }

    // Parse Content-Length
    size_t content_length = 0;
    size_t cl_pos = _headers.find("Content-Length:");
    if (cl_pos != std::string::npos) {
        size_t cl_end = _headers.find("\r\n", cl_pos);
        std::string cl_str = _headers.substr(cl_pos + 15, cl_end - (cl_pos + 15));
        content_length = std::stoi(cl_str);
    }

    // Read the rest of the body based on Content-Length
    size_t body_length = _body.size();
    while (body_length < content_length) {
        bytes_read = read(_client_fd, buffer, sizeof(buffer));
        if (bytes_read == -1) {
            std::cerr << "Error: read failed" << std::endl;
            close(_client_fd);
            exit(EXIT_FAILURE);
        } else if (bytes_read == 0) {
            // Client closed the connection unexpectedly
            break;
        }
        _body.append(buffer, bytes_read);
        body_length += bytes_read;
    }
}

std::string Request::unchunkRequestBody(const std::string &buffer) {
    std::istringstream stream(buffer);
    std::string line;
    std::string body;
    while (std::getline(stream, line)) {
        // Read chunk size in hexadecimal
        int chunkSize;
        std::stringstream hexStream(line);
        hexStream >> std::hex >> chunkSize;
        if (chunkSize == 0) {
            break; // Last chunk, stop reading
        }

        // Read the chunk data
        char *chunkData = new char[chunkSize + 1];
        stream.read(chunkData, chunkSize);
        chunkData[chunkSize] = '\0';
        body += std::string(chunkData);
        delete[] chunkData;

        // Skip the CRLF after the chunk
        std::getline(stream, line);
    }
    return body;
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
        char *const envp[] = {NULL}; // Environment variables
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

            // Pass the unchunked body to the CGI process via stdin
            if (!body.empty()) {
                write(STDIN_FILENO, body.c_str(), body.length());
            }

            // Optionally: Set a timeout for the child process
            alarm(2);  // Automatically kill the process after 2 seconds

            // Execute CGI script
            if (path.find(".py") != std::string::npos) {
                char *const pythonArgv[] = {(char *)"python3", (char *)path.c_str(), NULL};
                if (execve("/usr/bin/python3", pythonArgv, envp) == -1) {
                    std::cerr << "Failed to execute Python script" << std::endl;
                    exit(1);
                }
            } else {
                // General CGI script
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
    std::cout << _headers << std::endl;
    std::string::size_type pos = _headers.find("\r\n");

    if (pos != std::string::npos) {
        std::string requestLine = _headers.substr(0, pos);
        ParseLine(requestLine);

        std::cout << "Method: " << _method << std::endl;
        std::cout << "URL: " << _url << std::endl;
        std::cout << "HTTP Version: " << _http_version << std::endl << std::endl;

        if (isCgiRequest(_url)) {
            executeCGI(WWW_FOLD + _url, _method, _body);
        } else {
            SendResponse(_body); // Pass the body to SendResponse
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

void Request::SendResponse(const std::string &requestBody) {
    if (_method == "GET")
        GetResponse();
    else if (_method == "POST")
        PostResponse(requestBody);
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
