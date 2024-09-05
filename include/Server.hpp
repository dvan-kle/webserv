/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   Server.hpp                                         :+:    :+:            */
/*                                                     +:+                    */
/*   By: dvan-kle <dvan-kle@student.codam.nl>         +#+                     */
/*                                                   +#+                      */
/*   Created: 2024/07/26 15:32:25 by dvan-kle      #+#    #+#                 */
/*   Updated: 2024/08/01 15:27:00 by dvan-kle      ########   odam.nl         */
/*                                                                            */
/* ************************************************************************** */

#pragma once
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <unistd.h>
#include <cstring>

#define MAX_EVENTS 10

class Server
{
	private:
		int _sock;
		struct sockaddr_in _address;
	
		int _epoll_fd;
		struct epoll_event _event;
		struct epoll_event _events[MAX_EVENTS];

		int _conn_sock;
		
	public:
		Server(int port);
		Server(const Server &src) = delete;
		Server &operator=(const Server &src) = delete;
		~Server();

		void EpollCreate();
		void EpollWait();
		void HandleClient(int client_fd);
};

