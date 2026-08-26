#include "../includes/Client.hpp"

Client::Client(int fd, const std::string &ip)
	: _fd(fd), _ip(ip), _passOk(false), _registered(false), _closing(false) {}

int	Client::getFd() const { return (_fd); }

const std::string	&Client::getNickname() const { return (_nickname); }

const std::string	&Client::getUsername() const { return (_username); }

std::string	Client::prefix() const
{
	return (_nickname + "!" + _username + "@" + _ip);
}

bool	Client::isPassOk() const { return (_passOk); }

bool	Client::isRegistered() const { return (_registered); }

bool	Client::isClosing() const { return (_closing); }

void	Client::setNickname(const std::string &nick) { _nickname = nick; }

void	Client::setUsername(const std::string &user) { _username = user; }

void	Client::setPassOk(bool val) { _passOk = val; }

void	Client::setRegistered(bool val) { _registered = val; }

void	Client::setClosing(bool val) { _closing = val; }

void	Client::appendData(const char *data, size_t len)
{
	if (_in.size() + len > INPUT_LIMIT)
		_closing = true;
	else
		_in.append(data, len);
}

bool	Client::getNextLine(std::string &line)
{
	std::string::size_type	newline = _in.find('\n');

	if (newline == std::string::npos)
		return (false);
	line = _in.substr(0, newline);
	_in.erase(0, newline + 1);
	if (!line.empty() && line[line.size() - 1] == '\r')
		line.erase(line.size() - 1);
	return (true);
}

void	Client::queue(const std::string &msg)
{
	if (_out.size() + msg.size() > OUTPUT_LIMIT)
		_closing = true;
	else
		_out += msg + "\r\n";
}

bool	Client::hasOutput() const { return (!_out.empty()); }

const std::string	&Client::output() const { return (_out); }

void	Client::consumeOutput(size_t n) { _out.erase(0, n); }
