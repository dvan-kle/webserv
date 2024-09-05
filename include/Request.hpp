/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   Request.hpp                                        :+:    :+:            */
/*                                                     +:+                    */
/*   By: dvan-kle <dvan-kle@student.codam.nl>         +#+                     */
/*                                                   +#+                      */
/*   Created: 2024/08/01 15:40:39 by dvan-kle      #+#    #+#                 */
/*   Updated: 2024/09/05 17:18:48 by trstn4        ########   odam.nl         */
/*                                                                            */
/* ************************************************************************** */

#pragma once
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <sstream>
#include <sys/stat.h>

const std::string HTTP_200 = "200 OK\r\n";
const std::string HTTP_400 = "400 Bad Request\r\n";
const std::string HTTP_404 = "404 Not Found\r\n";
const std::string HTTP_405 = "405 Method Not Allowed\r\n";
const std::string HTTP_500 = "500 Internal Server Error\r\n";
const std::string CONTYPE_HTML = "Content-Type: text/html\r\n";
const std::string CON_TYPE_CSS = "Content-Type: text/css\r\n";
const std::string CONTENT_LENGTH = "Content-Length: ";
const std::string ERROR_FOLD = "error_pages";
const std::string WWW_FOLD = "www";

class Request
{
	private:
		int _client_fd;
		char _buffer[1024];
		
		std::string _method;
		std::string _url;
		std::string _http_version;
		
		std::string _request;
		std::string _response;

		std::string _body;
		
	
	public:
		Request(int client_fd);
		Request(const Request &src) = delete;
		Request &operator=(const Request &src) = delete;
		~Request();

		void ParseRequest();
		void ParseLine(std::string line);

		void SendResponse(std::string request);
		void GetResponse();
		void PostResponse(std::string request);
		
		void PageNotFound();
		void MethodNotAllowed();
		void createDir(std::string name);

		void executeCGI(std::string path, std::string method, std::string body);
		bool isCgiRequest(std::string path);
		
		std::string unchunkRequestBody(const std::string &buffer);
};
