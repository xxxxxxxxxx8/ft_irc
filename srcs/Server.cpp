#include "Server.hpp"

#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>

// Definition of the static flag. Starts at 0 (server running); the signal
// handler flips it to 1 to request a graceful shutdown.
volatile sig_atomic_t Server::_signal = 0;

Server::Server(int port, const std::string &password)
	: _port(port), _password(password), _listenFd(-1)
{
}

Server::~Server()
{
	if (_listenFd != -1)
		close(_listenFd);
}

// A signal handler must stay minimal and async-signal-safe: we only raise a
// flag here. The actual shutdown work happens back in run().
void Server::signalHandler(int signum)
{
	(void)signum;
	_signal = 1;
}

// Before a socket can communicate, it must be created and bound to a network
// address (address family, socket type and protocol). We create a non-blocking
// listening socket, then register it with poll().
void Server::setup()
{
	// Stop the server cleanly on Ctrl-C (SIGINT) or Ctrl-\ (SIGQUIT).
	signal(SIGINT, Server::signalHandler);
	signal(SIGQUIT, Server::signalHandler);

	_listenFd = socket(AF_INET, SOCK_STREAM, 0);
	if (_listenFd < 0)
		throw std::runtime_error("socket() failed");

	// Allow immediate reuse of the port after a restart.
	int flag = 1;
	if (setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0)
		throw std::runtime_error("setsockopt() failed");

	if (fcntl(_listenFd, F_SETFL, O_NONBLOCK) < 0)
		throw std::runtime_error("fcntl() failed");

	sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(_port);

	if (bind(_listenFd, (sockaddr *)&addr, sizeof(addr)) < 0)
		throw std::runtime_error("bind() failed");

	if (listen(_listenFd, SOMAXCONN) < 0)
		throw std::runtime_error("listen() failed");

	// Add the listening socket to poll so we get notified about new clients.
	pollfd pfd;
	pfd.fd = _listenFd;
	pfd.events = POLLIN; // notify us when the socket is readable
	pfd.revents = 0;
	_pollFds.push_back(pfd);

	std::cout << "Server listening on port " << _port << std::endl;
}

void Server::run()
{
	while (!_signal)
	{
		int ret = poll(_pollFds.data(), _pollFds.size(), -1);

		// A signal may interrupt poll(): leave the loop to shut down.
		if (_signal)
			break;
		if (ret < 0)
		{
			// EINTR just means a signal fired; it is not a real error.
			if (errno == EINTR)
				continue;
			throw std::runtime_error("poll() failed");
		}

		for (size_t i = 0; i < _pollFds.size(); ++i)
		{
			// revents == 0 means nothing happened on this socket.
			if (_pollFds[i].revents == 0)
				continue;
			if (_pollFds[i].fd == _listenFd)
				acceptClient();
			else if (_pollFds[i].revents & POLLIN)
				receiveData(_pollFds[i].fd);
		}
	}

	std::cout << "\nShutting down server..." << std::endl;
}

void Server::acceptClient()
{
	sockaddr_in	clientAddr;
	socklen_t	len = sizeof(clientAddr);

	int clientFd = accept(_listenFd, (sockaddr *)&clientAddr, &len);
	if (clientFd < 0)
		return;

	// Clients must be non-blocking too, otherwise a slow client could stall us.
	if (fcntl(clientFd, F_SETFL, O_NONBLOCK) < 0)
	{
		close(clientFd);
		return;
	}

	pollfd pfd;
	pfd.fd = clientFd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	_pollFds.push_back(pfd);

	std::cout << "New client connected: " << clientFd << std::endl;
}

void Server::receiveData(int fd)
{
	char buffer[512];
	std::memset(buffer, 0, sizeof(buffer));

	ssize_t bytes = recv(fd, buffer, sizeof(buffer) - 1, 0);
	// 0 means the client closed the connection; < 0 means a read error.
	if (bytes <= 0)
	{
		removeClient(fd);
		return;
	}

	std::cout << "Received from " << fd << ": " << buffer;
}

void Server::removeClient(int fd)
{
	close(fd);

	for (std::vector<struct pollfd>::iterator it = _pollFds.begin();
		 it != _pollFds.end();
		 ++it)
	{
		if (it->fd == fd)
		{
			_pollFds.erase(it);
			break;
		}
	}

	std::cout << "Client disconnected: " << fd << std::endl;
}
