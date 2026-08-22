#ifndef SERVER_HPP
# define SERVER_HPP

# include <csignal>
# include <map>
# include <string>
# include <vector>
# include <poll.h>

# define SERVER_NAME "ircserv"

struct Client
{
	int			fd;
	std::string	ip;
	std::string	buffer;
	std::string	nickname;
	std::string	username;
	bool		passOk;
	bool		registered;

	Client() : fd(-1), passOk(false), registered(false) {}
	Client(int fd, const std::string &ip)
		: fd(fd), ip(ip), passOk(false), registered(false) {}
};

class Server
{
	private:
		int								_port;
		std::string						_pass;
		int								_sockfd;
		std::vector<struct pollfd>		_pollfds;
		std::map<int, Client>			_clients;
		static volatile sig_atomic_t	_running;

		void	_removePollfd(int fd);

	public:
		Server(int port, const std::string &pass);
		~Server();

		void	init();
		void	loop();
		void	shutdown();

		static void	sigHandler(int sig);

		void	handleNewConnection();
		void	handleClientData(int fd);
		void	sendReply(int fd, const std::string &msg);
		void	dropClient(int fd, const std::string &reason);
};

#endif
