/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   Request.cpp                                        :+:    :+:            */
/*                                                     +:+                    */
/*   By: dvan-kle <dvan-kle@student.codam.nl>         +#+                     */
/*                                                   +#+                      */
/*   Created: 2024/08/01 15:42:12 by dvan-kle      #+#    #+#                 */
/*   Updated: 2024/08/09 15:06:34 by dvan-kle      ########   odam.nl         */
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
		//exit(EXIT_FAILURE);
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
	
	std::string request(_buffer);
	std::string::size_type pos = request.find("\r\n");
	if (pos != std::string::npos)
	{
		std::string requestLine = request.substr(0, pos);
		
		ParseLine(requestLine);

		std::cout << "Method: " << _method << std::endl;
		std::cout << "URL: " << _url << std::endl;
		std::cout << "HTTP Version: " << _http_version << std::endl << std::endl;
	} 
	else
		std::cerr << "Invalid HTTP request" << std::endl;
	
	SendResponse(request);
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
