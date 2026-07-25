#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <map>
#include <csignal>
#include <poll.h>

#define SERVER_NAME "ircserv"
#define BOT_NICK "marvin"

class Client;
class Channel;

class Server
{
	public:
		Server(int port, const std::string &password);
		~Server();

		// Prepare the listening socket and register signal handlers.
		void	setup();
		// Main event loop: waits on poll() until a signal asks us to stop.
		void	run();

		// Signal handler must be static so it can be used as a C callback.
		static void	signalHandler(int signum);

	private:
		Server(const Server &other);            // not implemented on purpose
		Server &operator=(const Server &other); // not implemented on purpose

		// Connection handling
		void	acceptClient();
		void	receiveData(int fd);
		void	sendData(int fd);
		void	disconnectClient(int fd, const std::string &reason);

		// Output helpers: every byte goes through the send buffer + POLLOUT.
		void	queueSend(Client *client, const std::string &message);
		void	reply(Client *client, const std::string &code, const std::string &params);
		void	broadcast(Channel *channel, const std::string &message, int excludeFd);
		void	setPollOut(int fd, bool enable);

		// Command parsing and dispatch
		void	handleLine(Client *client, const std::string &line);
		void	tryRegister(Client *client);

		void	cmdPass(Client *client, const std::vector<std::string> &params);
		void	cmdNick(Client *client, const std::vector<std::string> &params);
		void	cmdUser(Client *client, const std::vector<std::string> &params);
		void	cmdPing(Client *client, const std::vector<std::string> &params);
		void	cmdQuit(Client *client, const std::vector<std::string> &params);
		void	cmdPrivmsg(Client *client, const std::vector<std::string> &params,
					bool isNotice);
		void	cmdJoin(Client *client, const std::vector<std::string> &params);
		void	cmdPart(Client *client, const std::vector<std::string> &params);
		void	cmdKick(Client *client, const std::vector<std::string> &params);
		void	cmdInvite(Client *client, const std::vector<std::string> &params);
		void	cmdTopic(Client *client, const std::vector<std::string> &params);
		void	cmdMode(Client *client, const std::vector<std::string> &params);

		// Bonus: built-in bot reachable with PRIVMSG BOT_NICK
		void	botRespond(Client *client, const std::string &text);

		// Lookups
		Client	*findClientByNick(const std::string &nickname);
		Channel	*findChannel(const std::string &name);
		void	dropChannelIfEmpty(Channel *channel);
		void	sendNames(Client *client, Channel *channel);

		int									_port;
		std::string							_password;
		int									_listenFd;
		std::vector<struct pollfd>			_pollFds;
		std::map<int, Client *>				_clients;  // key: socket fd
		std::map<std::string, Channel *>	_channels; // key: lowercase name

		// Set to 1 by signalHandler() so run() can break out of poll() cleanly.
		static volatile sig_atomic_t	_signal;
};

std::string	toLowerStr(const std::string &s);

#endif
