/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   Socket.hpp                                         :+:    :+:            */
/*                                                     +:+                    */
/*   By: dvan-kle <dvan-kle@student.codam.nl>         +#+                     */
/*                                                   +#+                      */
/*   Created: 2024/07/26 15:32:25 by dvan-kle      #+#    #+#                 */
/*   Updated: 2024/07/26 15:50:56 by dvan-kle      ########   odam.nl         */
/*                                                                            */
/* ************************************************************************** */

#pragma once
#include <sys/socket.h>
#include <netinet/in.h>

class StartSocket
{
	private:
		int _sock;
		int _connection;
		struct sockaddr_in _address;
	public:
		StartSocket(int domain, int type, int protocol, int port, u_long interface);
		StartSocket(const StartSocket &src) = delete;
		StartSocket &operator=(const StartSocket &src) = delete;
		~StartSocket();

		virtual int establishConnection(int sock, struct sockaddr_in address) = 0;
};