#ifndef CLIENT_HPP
# define CLIENT_HPP

# include <cstddef>
# include <string>

# define MAX_LINE_LEN 512

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
		Client();
		Client(int fd, const std::string &ip);
		Client(const Client &src);
		Client	&operator=(const Client &rhs);
		~Client();

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
		bool	inputTooLong() const;

		void				queue(const std::string &msg);
		bool				hasOutput() const;
		const std::string	&output() const;
		void				consumeOutput(size_t n);
};

#endif
