/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   Request.hpp                                        :+:    :+:            */
/*                                                     +:+                    */
/*   By: dvan-kle <dvan-kle@student.codam.nl>         +#+                     */
/*                                                   +#+                      */
/*   Created: 2024/08/01 15:40:39 by dvan-kle      #+#    #+#                 */
/*   Updated: 2024/08/01 16:16:17 by dvan-kle      ########   odam.nl         */
/*                                                                            */
/* ************************************************************************** */

#pragma once
#include <unistd.h>
#include <iostream>
#include <cstring>

class Request
{
	private:
		int _client_fd;
		char _buffer[1024];
		std::string _method;
		std::string _url;
		std::string _http_version;
		
	
	public:
		Request(int client_fd);
		Request(const Request &src) = delete;
		Request &operator=(const Request &src) = delete;
		~Request();

		void ParseRequest();
		
		
};