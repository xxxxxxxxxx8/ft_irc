#include "../includes/Server.hpp"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

volatile sig_atomic_t Server::_running = 1;

/*	constructor / destructor	*/

Server::Server(int port, const std::string &pass)
	: _port(port), _pass(pass), _sockfd(-1) {}

Server::~Server() { shutdown(); }

/*	signal	*/

void	Server::sigHandler(int sig)
{
	(void)sig;
	_running = 0;
}

/*	setup	*/

void	Server::init()
{
	signal(SIGINT, Server::sigHandler);
	signal(SIGQUIT, Server::sigHandler);
	signal(SIGPIPE, SIG_IGN);

	_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (_sockfd < 0)
		throw std::runtime_error("socket() failed");

	int	opt = 1;
	if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
	{
		close(_sockfd);
		_sockfd = -1;
		throw std::runtime_error("setsockopt() failed");
	}
	if (fcntl(_sockfd, F_SETFL, O_NONBLOCK) < 0)
	{
		close(_sockfd);
		_sockfd = -1;
		throw std::runtime_error("fcntl() failed");
	}

	struct sockaddr_in	addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(_port);
	if (bind(_sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		close(_sockfd);
		_sockfd = -1;
		throw std::runtime_error("bind() failed – port probably in use");
	}
	if (listen(_sockfd, SOMAXCONN) < 0)
	{
		close(_sockfd);
		_sockfd = -1;
		throw std::runtime_error("listen() failed");
	}

	struct pollfd	pfd;
	pfd.fd = _sockfd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	_pollfds.push_back(pfd);

	std::cout << "Server listening on port " << _port << std::endl;
}

/*	main loop	*/

void	Server::loop()
{
	while (_running)
	{
		int ret = poll(&_pollfds[0], _pollfds.size(), -1);
		if (ret < 0)
		{
			if (!_running)
				break ;
			throw std::runtime_error("poll() failed");
		}
		for (size_t i = 0; i < _pollfds.size(); i++)
		{
			if (_pollfds[i].revents == 0)
				continue ;
			if (_pollfds[i].revents & (POLLHUP | POLLERR))
			{
				if (_pollfds[i].fd == _sockfd)
					throw std::runtime_error("listen socket error");
				dropClient(_pollfds[i].fd, "connection lost");
				i--;
				continue ;
			}
			if (_pollfds[i].revents & POLLIN)
			{
				if (_pollfds[i].fd == _sockfd)
					handleNewConnection();
				else
				{
					int fd = _pollfds[i].fd;
					handleClientData(fd);
					if (_clients.find(fd) == _clients.end())
						i--;
				}
			}
		}
	}
	std::cout << "\nShutting down..." << std::endl;
	shutdown();
}

/*	accept new client	*/

void	Server::handleNewConnection()
{
	struct sockaddr_in	cli_addr;
	socklen_t			cli_len = sizeof(cli_addr);

	int fd = accept(_sockfd, (struct sockaddr *)&cli_addr, &cli_len);
	if (fd < 0)
		return ;
	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
	{
		close(fd);
		return ;
	}

	struct pollfd	pfd;
	pfd.fd = fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	_pollfds.push_back(pfd);

	std::string ip = inet_ntoa(cli_addr.sin_addr);
	_clients.insert(std::make_pair(fd, Client(fd, ip)));
	std::cout << "New connection fd " << fd << " (" << ip << ")" << std::endl;
}

/*	read from client	*/

void	Server::handleClientData(int fd)
{
	char	buf[1024];
	std::memset(buf, 0, sizeof(buf));

	ssize_t	n = recv(fd, buf, sizeof(buf) - 1, 0);
	if (n <= 0)
	{
		dropClient(fd, n == 0 ? "client disconnected" : "recv() error");
		return ;
	}

	std::map<int, Client>::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return ;
	it->second.appendBuffer(std::string(buf, n));
	// TODO: parse buffer for complete messages (\r\n)
}

/*	send	*/

void	Server::sendReply(int fd, const std::string &msg)
{
	if (msg.empty())
		return ;
	if (send(fd, msg.c_str(), msg.length(), 0) < 0)
		std::cerr << "send() failed on fd " << fd << std::endl;
}

/*	remove pollfd entry	*/

void	Server::_removePollfd(int fd)
{
	for (std::vector<struct pollfd>::iterator it = _pollfds.begin();
		it != _pollfds.end(); ++it)
	{
		if (it->fd == fd)
		{
			_pollfds.erase(it);
			return ;
		}
	}
}

/*	disconnect a client	*/

void	Server::dropClient(int fd, const std::string &reason)
{
	std::cout << "Client fd " << fd << " dropped: " << reason << std::endl;
	close(fd);
	_removePollfd(fd);
	_clients.erase(fd);
}

/*	cleanup everything	*/

void	Server::shutdown()
{
	if (_sockfd == -1 && _clients.empty())
		return ;
	for (std::map<int, Client>::iterator it = _clients.begin();
		it != _clients.end(); ++it)
		close(it->first);
	_clients.clear();
	if (_sockfd != -1)
	{
		close(_sockfd);
		_sockfd = -1;
	}
	_pollfds.clear();
	std::cout << "Cleanup done." << std::endl;
}
