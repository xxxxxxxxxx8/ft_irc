#ifndef SERVER_HPP
# define SERVER_HPP

# include "Channel.hpp"
# include "Client.hpp"
# include <csignal>
# include <cstddef>
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
		std::map<std::string, Channel>	_channels;
		static volatile sig_atomic_t	_running;

		static std::string	toLower(const std::string &s);

		void	setPollEvents();
		void	handleEvents(int fd, short events);
		void	acceptClient();
		void	readClient(int fd);
		void	flushClient(int fd);
		void	dropClient(int fd);
		void	removePollfd(int fd);
		void	removeFromChannels(int fd);
		void	shutdown();

		void	reply(Client &client, const std::string &code,
				const std::string &text);
		void	broadcast(Channel &channel, const std::string &msg,
				int except = -1);
		void	broadcastToPeers(Client &client, const std::string &msg);

		Client	*findClientByNick(const std::string &nick);
		Channel	*findChannel(const std::string &name);

		void	handleLine(Client &client, const std::string &line);
		void	tryRegister(Client &client);
		void	sendJoinInfo(Client &client, Channel &channel);
		void	applyMode(Client &client, Channel &channel, char mode,
				bool adding, const std::vector<std::string> &params,
				size_t &argIndex);

		void	cmdPass(Client &client, const std::vector<std::string> &params);
		void	cmdNick(Client &client, const std::vector<std::string> &params);
		void	cmdUser(Client &client, const std::vector<std::string> &params);
		void	cmdPing(Client &client, const std::vector<std::string> &params);
		void	cmdPrivmsg(Client &client,
				const std::vector<std::string> &params);
		void	cmdJoin(Client &client, const std::vector<std::string> &params);
		void	cmdKick(Client &client, const std::vector<std::string> &params);
		void	cmdInvite(Client &client,
				const std::vector<std::string> &params);
		void	cmdTopic(Client &client, const std::vector<std::string> &params);
		void	cmdMode(Client &client, const std::vector<std::string> &params);

	public:
		Server(int port, const std::string &pass);
		~Server();

		void	init();
		void	run();

		static void	sigHandler(int sig);
};

#endif
