#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>

// One connected client: its socket, identity and I/O buffers.
class Client
{
	public:
		Client(int fd, const std::string &host);
		~Client();

		int					getFd() const;
		const std::string	&getNickname() const;
		const std::string	&getUsername() const;
		const std::string	&getHost() const;
		// "nick!user@host", used as message source prefix.
		std::string			prefix() const;

		void	setNickname(const std::string &nickname);
		void	setUsername(const std::string &username);
		void	setRealname(const std::string &realname);

		bool	isPassOk() const;
		void	setPassOk(bool ok);
		bool	isRegistered() const;
		void	setRegistered(bool registered);
		// A closing client is disconnected once its send buffer is flushed.
		bool	isClosing() const;
		void	setClosing(bool closing);

		// Incoming bytes are aggregated here until a full line is received.
		std::string	&recvBuffer();
		// Outgoing bytes wait here until poll() reports the socket writable.
		std::string	&sendBuffer();

	private:
		Client(const Client &other);
		Client &operator=(const Client &other);

		int			_fd;
		std::string	_host;
		std::string	_nickname;
		std::string	_username;
		std::string	_realname;
		std::string	_recvBuf;
		std::string	_sendBuf;
		bool		_passOk;
		bool		_registered;
		bool		_closing;
};

#endif
