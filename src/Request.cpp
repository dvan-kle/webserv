/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   Request.cpp                                        :+:    :+:            */
/*                                                     +:+                    */
/*   By: dvan-kle <dvan-kle@student.codam.nl>         +#+                     */
/*                                                   +#+                      */
/*   Created: 2024/08/01 15:42:12 by dvan-kle      #+#    #+#                 */
/*   Updated: 2024/08/01 17:38:18 by dvan-kle      ########   odam.nl         */
/*                                                                            */
/* ************************************************************************** */

#include "../incl/Request.hpp"

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
		exit(EXIT_FAILURE);
	}
	_buffer[bytes_read] = '\0';
	_www_path = "../www";
}

Request::~Request()
{
	close(_client_fd);
}

void Request::ParseRequest()
{
	std::cout << _buffer << std::endl;
	
	std::string request(_buffer);
	std::string::size_type pos = request.find("\r\n");
	if (pos != std::string::npos)
	{
		std::string requestLine = request.substr(0, pos);
		
		ParseLine(requestLine);

		std::cout << "Method: " << _method << std::endl;
		std::cout << "URL: " << _url << std::endl;
		std::cout << "HTTP Version: " << _http_version << std::endl;
	} 
	else
		std::cerr << "Invalid HTTP request" << std::endl;
	
	SendResponse();
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

void Request::SendResponse()
{
	_response += _http_version + " " + HTTP_200 + CONTENT_TYPE + CONTENT_LENGTH + "\r\n";
	std::cout << _response << std::endl;
	const char* httpResponse = _response.c_str();
    // "HTTP/1.1 200 OK\r\n"
    // "Content-Type: text/html; charset=UTF-8\r\n"
    // "Content-Length: sdasdsada\r\n"
	// "Server: webserv named: server\r\n"
    // "\r\n"
    // "<!DOCTYPE html>\n"
    // "<html>\n"
    // "<head>\n"
    // "    <title>Simple Response</title>\n"
    // "</head>\n"
    // "<body>\n"
    // "    <h1>Hello, World!</h1>\n"
    // "    <p>This is a simple HTML response from the server.</p>\n"
    // "</body>\n"
    // "</html>\n";
	write (_client_fd, "HTTP/1.1 200 OK\r\n", 17);
	write(_client_fd, httpResponse, strlen(httpResponse));
}