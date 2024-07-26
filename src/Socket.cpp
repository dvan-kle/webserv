/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   Socket.cpp                                         :+:    :+:            */
/*                                                     +:+                    */
/*   By: dvan-kle <dvan-kle@student.codam.nl>         +#+                     */
/*                                                   +#+                      */
/*   Created: 2024/07/26 15:32:01 by dvan-kle      #+#    #+#                 */
/*   Updated: 2024/07/26 15:50:24 by dvan-kle      ########   odam.nl         */
/*                                                                            */
/* ************************************************************************** */

#include "../incl/Socket.hpp"

StartSocket::StartSocket(int domain, int type, int protocol, int port, u_long interface)
{
	// Define address struct
	_address.sin_family = domain;
	_address.sin_port = htons(port);
	_address.sin_addr.s_addr = htonl(interface);
	// Make connection
	_sock = socket(domain, type, protocol);
	_connection = establishConnection(_sock, _address);
	
}