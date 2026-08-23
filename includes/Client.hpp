#ifndef CLIENT_HPP
# define CLIENT_HPP

# include <cstddef>
# include <string>

# define INPUT_LIMIT 8192
# define OUTPUT_LIMIT 1048576

class Client
{
	private:
		int			_fd;
		std::string	_ip;
		std::string	_in;
		std::string	_out;
		std::string	_nickname;
		std::string	_username;
		bool		_passOk;
		bool		_registered;
		bool		_closing;

	public:
		Client(int fd, const std::string &ip);

		int					getFd() const;
		const std::string	&getNickname() const;
		const std::string	&getUsername() const;
		std::string			prefix() const;

		bool	isPassOk() const;
		bool	isRegistered() const;
		bool	isClosing() const;

		void	setNickname(const std::string &nick);
		void	setUsername(const std::string &user);
		void	setPassOk(bool val);
		void	setRegistered(bool val);
		void	setClosing(bool val);

		void	appendData(const char *data, size_t len);
		bool	getNextLine(std::string &line);

		void				queue(const std::string &msg);
		bool				hasOutput() const;
		const std::string	&output() const;
		void				consumeOutput(size_t n);
};

#endif
