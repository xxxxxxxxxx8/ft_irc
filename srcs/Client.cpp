#include "../includes/Client.hpp"

Client::Client()
	: _fd(-1), _passOk(false), _registered(false), _closing(false) {}

Client::Client(int fd, const std::string &ip)
	: _fd(fd), _ip(ip), _passOk(false), _registered(false), _closing(false) {}

Client::Client(const Client &src)
	: _fd(src._fd), _ip(src._ip), _in(src._in), _out(src._out),
	  _nickname(src._nickname), _username(src._username),
	  _passOk(src._passOk),
	  _registered(src._registered), _closing(src._closing) {}

Client	&Client::operator=(const Client &rhs)
{
	if (this != &rhs)
	{
		_fd = rhs._fd;
		_ip = rhs._ip;
		_in = rhs._in;
		_out = rhs._out;
		_nickname = rhs._nickname;
		_username = rhs._username;
		_passOk = rhs._passOk;
		_registered = rhs._registered;
		_closing = rhs._closing;
	}
	return (*this);
}

Client::~Client() {}

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
	_in.append(data, len);
}

bool	Client::getNextLine(std::string &line)
{
	std::string::size_type	pos = _in.find('\n');

	if (pos == std::string::npos)
		return (false);
	line = _in.substr(0, pos);
	_in.erase(0, pos + 1);
	if (!line.empty() && line[line.size() - 1] == '\r')
		line.erase(line.size() - 1);
	return (true);
}

bool	Client::inputTooLong() const
{
	return (_in.size() > MAX_LINE_LEN);
}

void	Client::queue(const std::string &msg)
{
	_out += msg + "\r\n";
}

bool	Client::hasOutput() const { return (!_out.empty()); }

const std::string	&Client::output() const { return (_out); }

void	Client::consumeOutput(size_t n) { _out.erase(0, n); }
