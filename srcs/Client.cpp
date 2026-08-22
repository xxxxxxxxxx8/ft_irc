#include "../includes/Client.hpp"

Client::Client()
	: _fd(-1), _passOk(false), _registered(false) {}

Client::Client(int fd, const std::string &ip)
	: _fd(fd), _ip(ip), _passOk(false), _registered(false) {}

Client::Client(const Client &src)
	: _fd(src._fd), _ip(src._ip), _buffer(src._buffer),
	  _nickname(src._nickname), _username(src._username),
	  _realname(src._realname), _passOk(src._passOk),
	  _registered(src._registered) {}

Client &Client::operator=(const Client &rhs)
{
	if (this != &rhs)
	{
		_fd = rhs._fd;
		_ip = rhs._ip;
		_buffer = rhs._buffer;
		_nickname = rhs._nickname;
		_username = rhs._username;
		_realname = rhs._realname;
		_passOk = rhs._passOk;
		_registered = rhs._registered;
	}
	return (*this);
}

Client::~Client() {}

int	Client::getFd() const { return (_fd); }

const std::string	&Client::getIp() const { return (_ip); }

const std::string	&Client::getBuffer() const { return (_buffer); }

const std::string	&Client::getNickname() const { return (_nickname); }

const std::string	&Client::getUsername() const { return (_username); }

const std::string	&Client::getRealname() const { return (_realname); }

bool	Client::hasPassword() const { return (_passOk); }

bool	Client::isRegistered() const { return (_registered); }

void	Client::setNickname(const std::string &nick) { _nickname = nick; }

void	Client::setUsername(const std::string &user) { _username = user; }

void	Client::setRealname(const std::string &real) { _realname = real; }

void	Client::setHasPassword(bool val) { _passOk = val; }

void	Client::setRegistered(bool val) { _registered = val; }

void	Client::setBuffer(const std::string &buf) { _buffer = buf; }

void	Client::appendBuffer(const std::string &data) { _buffer += data; }

void	Client::clearBuffer() { _buffer.clear(); }
