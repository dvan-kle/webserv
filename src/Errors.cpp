#include "../include/Request.hpp"

void Request::PageNotFound()
{
	std::string responsefile  = ERROR_FOLD + "/error_404.html";
	std::ifstream ifstr(responsefile, std::ios::binary);
	if (!ifstr)
	{
		std::cerr << "Error: 404.html not found" << std::endl;
		close(_client_fd);
		exit(EXIT_FAILURE);
	}
	std::string htmlContent((std::istreambuf_iterator<char>(ifstr)), std::istreambuf_iterator<char>());
	_response += _http_version + " " + HTTP_404 + CONTYPE_HTML;
	_response += CONTENT_LENGTH + std::to_string(htmlContent.size()) + "\r\n\r\n";
	_response += htmlContent;
	
	ssize_t bytes_written = write(_client_fd, _response.c_str(), strlen(_response.c_str()));
	if (bytes_written == -1)
	{
		std::cerr << "Error: write failed" << std::endl;
		close(_client_fd);
		exit(EXIT_FAILURE);
	}
}

void Request::MethodNotAllowed()
{
	std::string responsefile  = ERROR_FOLD + "/error_404.html";
	std::ifstream ifstr(responsefile, std::ios::binary);
	if (!ifstr)
	{
		std::cerr << "Error: 404.html not found" << std::endl;
		close(_client_fd);
		exit(EXIT_FAILURE);
	}
	std::string htmlContent((std::istreambuf_iterator<char>(ifstr)), std::istreambuf_iterator<char>());
	_response += _http_version + " " + HTTP_404 + CONTYPE_HTML;
	_response += CONTENT_LENGTH + std::to_string(htmlContent.size()) + "\r\n\r\n";
	_response += htmlContent;
	
	ssize_t bytes_written = write(_client_fd, _response.c_str(), strlen(_response.c_str()));
	if (bytes_written == -1)
	{
		std::cerr << "Error: write failed" << std::endl;
		close(_client_fd);
		exit(EXIT_FAILURE);
	}
}