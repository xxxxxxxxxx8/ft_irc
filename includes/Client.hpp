#ifndef CLIENT_HPP
# define CLIENT_HPP

# include <string>

class Client
{
	private:
		int			_fd;
		std::string	_ip;
		std::string	_buffer;
		std::string	_nickname;
		std::string	_username;
		std::string	_realname;
		bool		_passOk;
		bool		_registered;

	public:
		Client();
		Client(int fd, const std::string &ip);
		Client(const Client &src);
		Client &operator=(const Client &rhs);
		~Client();

		int					getFd() const;
		const std::string	&getIp() const;
		const std::string	&getBuffer() const;
		const std::string	&getNickname() const;
		const std::string	&getUsername() const;
		const std::string	&getRealname() const;
		bool				hasPassword() const;
		bool				isRegistered() const;

		void	setNickname(const std::string &nick);
		void	setUsername(const std::string &user);
		void	setRealname(const std::string &real);
		void	setHasPassword(bool val);
		void	setRegistered(bool val);
		void	setBuffer(const std::string &buf);

		void	appendBuffer(const std::string &data);
		void	clearBuffer();
};

#endif
