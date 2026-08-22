#ifndef SERVER_HPP
# define SERVER_HPP

# include "Client.hpp"
# include <csignal>
# include <map>
# include <string>
# include <vector>
# include <poll.h>

# define SERVER_NAME "ircserv"

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
