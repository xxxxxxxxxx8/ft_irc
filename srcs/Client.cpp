#include "Client.hpp"

Client::Client(int fd, const std::string &host)
	: _fd(fd), _host(host), _passOk(false), _registered(false), _closing(false)
{
}

Client::~Client()
{
}

int Client::getFd() const
{
	return _fd;
}

const std::string &Client::getNickname() const
{
	return _nickname;
}

const std::string &Client::getUsername() const
{
	return _username;
}

const std::string &Client::getHost() const
{
	return _host;
}

std::string Client::prefix() const
{
	return _nickname + "!" + _username + "@" + _host;
}

void Client::setNickname(const std::string &nickname)
{
	_nickname = nickname;
}

void Client::setUsername(const std::string &username)
{
	_username = username;
}

void Client::setRealname(const std::string &realname)
{
	_realname = realname;
}

bool Client::isPassOk() const
{
	return _passOk;
}

void Client::setPassOk(bool ok)
{
	_passOk = ok;
}

bool Client::isRegistered() const
{
	return _registered;
}

void Client::setRegistered(bool registered)
{
	_registered = registered;
}

bool Client::isClosing() const
{
	return _closing;
}

void Client::setClosing(bool closing)
{
	_closing = closing;
}

std::string &Client::recvBuffer()
{
	return _recvBuf;
}

std::string &Client::sendBuffer()
{
	return _sendBuf;
}
