/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   main.cpp                                           :+:    :+:            */
/*                                                     +:+                    */
/*   By: dvan-kle <dvan-kle@student.codam.nl>         +#+                     */
/*                                                   +#+                      */
/*   Created: 2024/05/31 14:35:09 by dvan-kle      #+#    #+#                 */
/*   Updated: 2024/07/26 16:00:30 by trstn4        ########   odam.nl         */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

int main(int ac, char **av)
{
	
	int server_fd, new_socket;
	
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
}
