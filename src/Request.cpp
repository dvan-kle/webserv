/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   Request.cpp                                        :+:    :+:            */
/*                                                     +:+                    */
/*   By: dvan-kle <dvan-kle@student.codam.nl>         +#+                     */
/*                                                   +#+                      */
/*   Created: 2024/08/01 15:42:12 by dvan-kle      #+#    #+#                 */
/*   Updated: 2024/08/01 16:16:11 by dvan-kle      ########   odam.nl         */
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
}

Request::~Request()
{
	close(_client_fd);
}

void Request::ParseRequest()
{
	std::cout << _buffer << std::endl;
	const char* httpResponse = 
    "HTTP/1.1 400 Bad Request\r\n"
    "Content-Type: text/html; charset=UTF-8\r\n"
    "Content-Length: sdasdsada\r\n"
    "\r\n"
    "<!DOCTYPE html>\n"
    "<html>\n"
    "<head>\n"
    "    <title>Simple Response</title>\n"
    "</head>\n"
    "<body>\n"
    "    <h1>Hello, World!</h1>\n"
    "    <p>This is a simple HTML response from the server.</p>\n"
    "</body>\n"
    "</html>\n";
	write(_client_fd, httpResponse, strlen(httpResponse));
}