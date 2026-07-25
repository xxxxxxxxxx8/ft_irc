#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "Debug.hpp"

#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cctype>
#include <cerrno>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

// Definition of the static flag. Starts at 0 (server running); the signal
// handler flips it to 1 to request a graceful shutdown.
volatile sig_atomic_t Server::_signal = 0;

std::string toLowerStr(const std::string &s)
{
	std::string out(s);
	for (size_t i = 0; i < out.size(); ++i)
		out[i] = std::tolower(static_cast<unsigned char>(out[i]));
	return out;
}

Server::Server(int port, const std::string &password)
	: _port(port), _password(password), _listenFd(-1)
{
}

Server::~Server()
{
	for (std::map<int, Client *>::iterator it = _clients.begin();
		 it != _clients.end(); ++it)
	{
		close(it->first);
		delete it->second;
	}
	for (std::map<std::string, Channel *>::iterator it = _channels.begin();
		 it != _channels.end(); ++it)
		delete it->second;
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

// Create a non-blocking listening socket bound to the given port, then
// register it with poll().
void Server::setup()
{
	DBG_FUNC(); // [debug] workflow step 1: build the listening socket

	// Stop the server cleanly on Ctrl-C (SIGINT) or Ctrl-\ (SIGQUIT), and
	// ignore SIGPIPE so a write to a closed socket cannot kill the server.
	signal(SIGINT, Server::signalHandler);
	signal(SIGQUIT, Server::signalHandler);
	signal(SIGPIPE, SIG_IGN);
	DBG("signal handlers installed (SIGINT/SIGQUIT stop, SIGPIPE ignored)");

	_listenFd = socket(AF_INET, SOCK_STREAM, 0);
	if (_listenFd < 0)
	{
		// [debug] usually: process ran out of file descriptors (EMFILE)
		DBG_ERR("socket() failed");
		throw std::runtime_error("socket() failed");
	}
	DBG("socket() ok, listen fd=" << _listenFd);

	// Allow immediate reuse of the port after a restart.
	int flag = 1;
	if (setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0)
	{
		DBG_ERR("setsockopt(SO_REUSEADDR) failed");
		throw std::runtime_error("setsockopt() failed");
	}

	if (fcntl(_listenFd, F_SETFL, O_NONBLOCK) < 0)
	{
		DBG_ERR("fcntl(O_NONBLOCK) failed");
		throw std::runtime_error("fcntl() failed");
	}
	DBG("listen socket set to non-blocking mode");

	sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(_port);

	if (bind(_listenFd, (sockaddr *)&addr, sizeof(addr)) < 0)
	{
		// [debug] most common cause: port already taken by another process
		// (EADDRINUSE) -> the trace prints "Address already in use"
		DBG_ERR("bind() failed on port " << _port);
		throw std::runtime_error("bind() failed");
	}
	DBG("bind() ok on port " << _port);

	if (listen(_listenFd, SOMAXCONN) < 0)
	{
		DBG_ERR("listen() failed");
		throw std::runtime_error("listen() failed");
	}
	DBG("listen() ok, backlog=" << SOMAXCONN);

	pollfd pfd;
	pfd.fd = _listenFd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	_pollFds.push_back(pfd);

	std::cout << "Server listening on port " << _port << std::endl;
}

void Server::run()
{
	DBG_FUNC(); // [debug] workflow step 2: enter the event loop

	while (!_signal)
	{
		// [debug] poll() SLEEPS here until one of the fds has activity
		// (new connection, data to read, or room to write).
		int ret = poll(&_pollFds[0], _pollFds.size(), -1);

		// A signal may interrupt poll(): leave the loop to shut down.
		if (_signal)
			break;
		if (ret < 0)
		{
			// EINTR just means a signal fired; it is not a real error.
			if (errno == EINTR)
				continue;
			DBG_ERR("poll() failed"); // [debug] real poll failure (why?)
			throw std::runtime_error("poll() failed");
		}
		DBG_EVENT("poll() woke up: " << ret << " fd(s) ready out of "
			<< _pollFds.size());

		// Work on a snapshot: handlers may add/remove entries in _pollFds.
		std::vector<struct pollfd> ready(_pollFds);
		for (size_t i = 0; i < ready.size(); ++i)
		{
			if (ready[i].revents == 0)
				continue;
			int fd = ready[i].fd;
			if (fd == _listenFd)
			{
				if (ready[i].revents & POLLIN)
					acceptClient();
				continue;
			}
			// The client may have been removed earlier in this loop.
			if (_clients.find(fd) == _clients.end())
				continue;
			if (ready[i].revents & (POLLERR | POLLHUP | POLLNVAL))
			{
				// [debug] POLLERR = socket error, POLLHUP = peer hung up,
				// POLLNVAL = fd not open -> in all cases the client is dead.
				DBG_EVENT("fd " << fd << " reported POLLERR/POLLHUP/POLLNVAL");
				disconnectClient(fd, "Connection error");
				continue;
			}
			if (ready[i].revents & POLLIN)
				receiveData(fd);
			if (_clients.find(fd) != _clients.end()
				&& (ready[i].revents & POLLOUT))
				sendData(fd);
		}
	}

	std::cout << "\nShutting down server..." << std::endl;
}

void Server::acceptClient()
{
	DBG_FUNC(); // [debug] workflow step 3: a new client knocked on the door

	sockaddr_in	clientAddr;
	socklen_t	len = sizeof(clientAddr);

	int clientFd = accept(_listenFd, (sockaddr *)&clientAddr, &len);
	if (clientFd < 0)
	{
		// [debug] not fatal: the client may already be gone (ECONNABORTED)
		// or accept would block (EAGAIN); we simply wait for the next one.
		DBG_ERR("accept() returned -1, connection dropped");
		return;
	}

	// Clients must be non-blocking too, otherwise a slow client could stall us.
	if (fcntl(clientFd, F_SETFL, O_NONBLOCK) < 0)
	{
		DBG_ERR("fcntl(O_NONBLOCK) failed for new client fd " << clientFd);
		close(clientFd);
		return;
	}

	pollfd pfd;
	pfd.fd = clientFd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	_pollFds.push_back(pfd);

	_clients[clientFd] = new Client(clientFd, inet_ntoa(clientAddr.sin_addr));

	DBG_EVENT("client accepted: fd=" << clientFd << " host="
		<< inet_ntoa(clientAddr.sin_addr) << " (total clients: "
		<< _clients.size() << ")");
	std::cout << "New client connected: " << clientFd << std::endl;
}

// Append incoming bytes to the client's buffer, then extract and handle every
// complete line. This rebuilds commands sent in several parts (nc + ctrl+D).
void Server::receiveData(int fd)
{
	char buffer[512];

	ssize_t bytes = recv(fd, buffer, sizeof(buffer), 0);
	// 0 means the client closed the connection; < 0 means a read error.
	if (bytes <= 0)
	{
		// [debug] bytes == 0 -> clean close (client typed /quit or ctrl+C);
		// bytes < 0 -> read error, strerror explains which one.
		if (bytes == 0)
			DBG_EVENT("fd " << fd << " closed the connection (recv == 0)");
		else
			DBG_ERR("recv() failed on fd " << fd);
		disconnectClient(fd, "Connection closed");
		return;
	}

	Client *client = _clients[fd];
	client->recvBuffer().append(buffer, bytes);
	DBG("recv " << bytes << " byte(s) from fd " << fd << ", buffer now "
		<< client->recvBuffer().size() << " byte(s)");

	size_t pos;
	while ((pos = client->recvBuffer().find('\n')) != std::string::npos)
	{
		std::string line = client->recvBuffer().substr(0, pos);
		client->recvBuffer().erase(0, pos + 1);
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);

		DBG_IN(fd, line); // [debug] one full IRC line rebuilt from the buffer
		handleLine(client, line);
		// QUIT (or an error) may have destroyed the client.
		if (_clients.find(fd) == _clients.end() || client->isClosing())
			return;
	}

	// A line without terminator may never exceed the IRC message size limit.
	if (client->recvBuffer().size() > 512)
	{
		// [debug] protection against a client flooding bytes with no '\n'
		DBG_EVENT("fd " << fd << " exceeded 512 bytes without newline");
		disconnectClient(fd, "Input line too long");
	}
}

// Flush as much of the send buffer as the socket accepts right now. Only
// called when poll() reported the socket writable.
void Server::sendData(int fd)
{
	Client *client = _clients[fd];
	std::string &buf = client->sendBuffer();

	ssize_t bytes = send(fd, buf.c_str(), buf.size(), 0);
	if (bytes > 0)
	{
		buf.erase(0, bytes);
		DBG("sent " << bytes << " byte(s) to fd " << fd << ", "
			<< buf.size() << " byte(s) still queued");
	}
	else if (bytes < 0)
		DBG_ERR("send() failed on fd " << fd); // [debug] kernel buffer full?
	if (buf.empty())
	{
		// [debug] nothing left to write: stop asking poll() for POLLOUT,
		// otherwise poll() would wake up in a busy loop forever.
		setPollOut(fd, false);
		if (client->isClosing())
			disconnectClient(fd, "Closing link");
	}
}

void Server::disconnectClient(int fd, const std::string &reason)
{
	std::map<int, Client *>::iterator cit = _clients.find(fd);
	if (cit == _clients.end())
		return;
	Client *client = cit->second;

	DBG_EVENT("disconnecting fd " << fd << " nick='" << client->getNickname()
		<< "' reason: " << reason);

	// Tell every peer sharing a channel with this client that it quit
	// (each peer only once, even if several channels are shared).
	std::map<int, Client *> peers;
	std::map<std::string, Channel *>::iterator it = _channels.begin();
	while (it != _channels.end())
	{
		Channel *channel = it->second;
		++it;
		if (!channel->isMember(fd))
			continue;
		const std::map<int, Client *> &members = channel->getMembers();
		for (std::map<int, Client *>::const_iterator m = members.begin();
			 m != members.end(); ++m)
			if (m->first != fd)
				peers[m->first] = m->second;
		channel->removeMember(fd);
		dropChannelIfEmpty(channel);
	}
	std::string quitMsg = ":" + client->prefix() + " QUIT :" + reason;
	for (std::map<int, Client *>::iterator p = peers.begin();
		 p != peers.end(); ++p)
		queueSend(p->second, quitMsg);

	close(fd);
	for (std::vector<struct pollfd>::iterator pit = _pollFds.begin();
		 pit != _pollFds.end(); ++pit)
	{
		if (pit->fd == fd)
		{
			_pollFds.erase(pit);
			break;
		}
	}
	delete client;
	_clients.erase(fd);

	std::cout << "Client disconnected: " << fd << " (" << reason << ")"
			  << std::endl;
}

// Queue a message; it is written by sendData() once poll() says the socket
// is writable. The IRC line terminator is appended here.
void Server::queueSend(Client *client, const std::string &message)
{
	DBG_OUT(client->getFd(), message); // [debug] every outgoing IRC line
	client->sendBuffer() += message + "\r\n";
	setPollOut(client->getFd(), true);
}

// Numeric reply: ":<server> <code> <nick> <params>"
void Server::reply(Client *client, const std::string &code,
	const std::string &params)
{
	std::string nick = client->getNickname().empty() ? "*"
													 : client->getNickname();
	queueSend(client, ":" SERVER_NAME " " + code + " " + nick + " " + params);
}

void Server::broadcast(Channel *channel, const std::string &message,
	int excludeFd)
{
	const std::map<int, Client *> &members = channel->getMembers();
	for (std::map<int, Client *>::const_iterator it = members.begin();
		 it != members.end(); ++it)
		if (it->first != excludeFd)
			queueSend(it->second, message);
}

void Server::setPollOut(int fd, bool enable)
{
	for (size_t i = 0; i < _pollFds.size(); ++i)
	{
		if (_pollFds[i].fd == fd)
		{
			if (enable)
				_pollFds[i].events = POLLIN | POLLOUT;
			else
				_pollFds[i].events = POLLIN;
			return;
		}
	}
}

Client *Server::findClientByNick(const std::string &nickname)
{
	std::string lower = toLowerStr(nickname);
	for (std::map<int, Client *>::iterator it = _clients.begin();
		 it != _clients.end(); ++it)
		if (toLowerStr(it->second->getNickname()) == lower)
			return it->second;
	return NULL;
}

Channel *Server::findChannel(const std::string &name)
{
	std::map<std::string, Channel *>::iterator it =
		_channels.find(toLowerStr(name));
	if (it == _channels.end())
		return NULL;
	return it->second;
}

void Server::dropChannelIfEmpty(Channel *channel)
{
	if (!channel->isEmpty())
		return;
	_channels.erase(toLowerStr(channel->getName()));
	delete channel;
}
