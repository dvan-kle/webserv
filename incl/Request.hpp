/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   Request.hpp                                        :+:    :+:            */
/*                                                     +:+                    */
/*   By: dvan-kle <dvan-kle@student.codam.nl>         +#+                     */
/*                                                   +#+                      */
/*   Created: 2024/08/01 15:40:39 by dvan-kle      #+#    #+#                 */
/*   Updated: 2024/08/01 17:37:15 by dvan-kle      ########   odam.nl         */
/*                                                                            */
/* ************************************************************************** */

#pragma once
#include <unistd.h>
#include <iostream>
#include <cstring>

const std::string HTTP_200 = "200 OK\r\n";
const std::string HTTP_400 = "400 Bad Request\r\n";
const std::string HTTP_404 = "404 Not Found\r\n";
const std::string HTTP_405 = "405 Method Not Allowed\r\n";
const std::string HTTP_500 = "500 Internal Server Error\r\n";
const std::string CONTENT_TYPE = "Content-Type: text/html\r\n";
const std::string CONTENT_LENGTH = "Content-Length: ";




class Request
{
	private:
		int _client_fd;
		char _buffer[1024];
		
		std::string _method;
		std::string _url;
		std::string _http_version;
		
		std::string _www_path;
		std::string _response;
		
	
	public:
		Request(int client_fd);
		Request(const Request &src) = delete;
		Request &operator=(const Request &src) = delete;
		~Request();

		void ParseRequest();
		void ParseLine(std::string line);

		void SendResponse();
		
		
};