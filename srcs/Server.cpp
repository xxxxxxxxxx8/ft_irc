#include "../includes/Server.hpp"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

volatile sig_atomic_t Server::_running = 1;

Server::Server(int port, const std::string &pass)
	: _port(port), _pass(pass), _sockfd(-1) {}

Server::~Server() { shutdown(); }

void	Server::sigHandler(int sig)
{
	(void)sig;
	_running = 0;
}

std::string	Server::toLower(const std::string &s)
{
	std::string	out(s);

	for (size_t i = 0; i < out.size(); i++)
	{
		if (out[i] >= 'A' && out[i] <= 'Z')
			out[i] = static_cast<char>(out[i] + 32);
	}
	return (out);
}

void	Server::init()
{
	struct sockaddr_in	addr;
	struct pollfd		pfd;
	int					opt = 1;

	signal(SIGINT, Server::sigHandler);
	signal(SIGQUIT, Server::sigHandler);
	signal(SIGPIPE, SIG_IGN);

	_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (_sockfd < 0)
		throw std::runtime_error("socket() failed");
	if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
		throw std::runtime_error("setsockopt() failed");
	if (fcntl(_sockfd, F_SETFL, O_NONBLOCK) < 0)
		throw std::runtime_error("fcntl() failed");

	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(static_cast<uint16_t>(_port));
	if (bind(_sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		throw std::runtime_error("bind() failed, port in use or not permitted");
	if (listen(_sockfd, SOMAXCONN) < 0)
		throw std::runtime_error("listen() failed");

	pfd.fd = _sockfd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	_pollfds.push_back(pfd);
	std::cout << "Listening on port " << _port << std::endl;
}

void	Server::updateEvents()
{
	for (size_t i = 0; i < _pollfds.size(); i++)
	{
		std::map<int, Client>::iterator	it = _clients.find(_pollfds[i].fd);

		if (it == _clients.end())
		{
			_pollfds[i].events = POLLIN;
			continue ;
		}
		_pollfds[i].events = it->second.isClosing() ? 0 : POLLIN;
		if (it->second.hasOutput())
			_pollfds[i].events |= POLLOUT;
	}
}

void	Server::run()
{
	while (_running)
	{
		updateEvents();
		if (poll(&_pollfds[0], _pollfds.size(), -1) < 0)
		{
			if (!_running)
				break ;
			continue ;
		}
		for (size_t i = 0; i < _pollfds.size(); i++)
		{
			short	events = _pollfds[i].revents;
			int		fd = _pollfds[i].fd;

			if (events == 0)
				continue ;
			try
			{
				handleEvents(fd, events);
			}
			catch (const std::exception &e)
			{
				std::cerr << "fd " << fd << ": " << e.what() << std::endl;
				if (fd != _sockfd)
					dropClient(fd);
			}
			if (fd != _sockfd && _clients.count(fd) == 0)
				i--;
		}
	}
	shutdown();
}

void	Server::handleEvents(int fd, short events)
{
	if (fd == _sockfd)
	{
		if (events & POLLIN)
			acceptClient();
		return ;
	}
	if (events & POLLIN)
		readClient(fd);
	if (_clients.count(fd) && (events & POLLOUT))
		flushClient(fd);
	if (_clients.count(fd) && (events & (POLLERR | POLLHUP | POLLNVAL)))
		dropClient(fd);
}

void	Server::acceptClient()
{
	struct sockaddr_in	addr;
	socklen_t			len = sizeof(addr);
	struct pollfd		pfd;

	int	fd = accept(_sockfd, (struct sockaddr *)&addr, &len);
	if (fd < 0)
		return ;
	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
	{
		close(fd);
		return ;
	}
	pfd.fd = fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	_pollfds.push_back(pfd);

	std::string	ip = inet_ntoa(addr.sin_addr);
	_clients.insert(std::make_pair(fd, Client(fd, ip)));
	std::cout << "fd " << fd << " connected from " << ip << std::endl;
}

void	Server::readClient(int fd)
{
	char	buf[1024];
	ssize_t	n = recv(fd, buf, sizeof(buf), 0);

	if (n <= 0)
	{
		dropClient(fd);
		return ;
	}

	std::map<int, Client>::iterator	it = _clients.find(fd);
	if (it == _clients.end())
		return ;

	Client		&client = it->second;
	std::string	line;

	client.appendData(buf, static_cast<size_t>(n));
	while (!client.isClosing() && client.getNextLine(line))
		handleLine(client, line);
	if (client.inputTooLong()
		|| (client.isClosing() && !client.hasOutput()))
		dropClient(fd);
}

void	Server::flushClient(int fd)
{
	std::map<int, Client>::iterator	it = _clients.find(fd);

	if (it == _clients.end())
		return ;

	Client	&client = it->second;
	ssize_t	n = send(fd, client.output().c_str(), client.output().size(), 0);

	if (n <= 0)
	{
		dropClient(fd);
		return ;
	}
	client.consumeOutput(static_cast<size_t>(n));
	if (client.isClosing() && !client.hasOutput())
		dropClient(fd);
}

void	Server::removeFromChannels(int fd)
{
	std::map<std::string, Channel>::iterator	it = _channels.begin();

	while (it != _channels.end())
	{
		it->second.removeMember(fd);
		if (it->second.isEmpty())
			_channels.erase(it++);
		else
			++it;
	}
}

void	Server::dropClient(int fd)
{
	std::map<int, Client>::iterator	it = _clients.find(fd);

	if (it != _clients.end())
	{
		if (it->second.isRegistered())
			broadcastToPeers(it->second, ":" + it->second.prefix()
				+ " QUIT :Connection closed");
		removeFromChannels(fd);
		std::cout << "fd " << fd << " disconnected" << std::endl;
	}
	close(fd);
	removePollfd(fd);
	_clients.erase(fd);
}

void	Server::removePollfd(int fd)
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

void	Server::reply(Client &client, const std::string &code,
	const std::string &text)
{
	std::string	nick = client.getNickname();

	if (nick.empty())
		nick = "*";
	client.queue(":" SERVER_NAME " " + code + " " + nick + " " + text);
}

void	Server::broadcast(Channel &channel, const std::string &msg, int except)
{
	const std::set<int>	&members = channel.getMembers();

	for (std::set<int>::const_iterator it = members.begin();
		it != members.end(); ++it)
	{
		std::map<int, Client>::iterator	c = _clients.find(*it);

		if (*it != except && c != _clients.end())
			c->second.queue(msg);
	}
}

void	Server::broadcastToPeers(Client &client, const std::string &msg)
{
	std::set<int>	targets;

	for (std::map<std::string, Channel>::iterator it = _channels.begin();
		it != _channels.end(); ++it)
	{
		if (!it->second.isMember(client.getFd()))
			continue ;
		targets.insert(it->second.getMembers().begin(),
			it->second.getMembers().end());
	}
	targets.erase(client.getFd());
	for (std::set<int>::iterator it = targets.begin();
		it != targets.end(); ++it)
	{
		std::map<int, Client>::iterator	c = _clients.find(*it);

		if (c != _clients.end())
			c->second.queue(msg);
	}
}

Client	*Server::findClientByNick(const std::string &nick)
{
	std::string	target = toLower(nick);

	if (target.empty())
		return (NULL);
	for (std::map<int, Client>::iterator it = _clients.begin();
		it != _clients.end(); ++it)
	{
		if (toLower(it->second.getNickname()) == target)
			return (&it->second);
	}
	return (NULL);
}

Channel	*Server::findChannel(const std::string &name)
{
	std::map<std::string, Channel>::iterator	it;

	it = _channels.find(toLower(name));
	if (it == _channels.end())
		return (NULL);
	return (&it->second);
}

void	Server::shutdown()
{
	for (std::map<int, Client>::iterator it = _clients.begin();
		it != _clients.end(); ++it)
		close(it->first);
	_clients.clear();
	_channels.clear();
	_pollfds.clear();
	if (_sockfd != -1)
	{
		close(_sockfd);
		_sockfd = -1;
	}
}
