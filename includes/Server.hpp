#ifndef SERVER_HPP
# define SERVER_HPP

# include <string>
# include <vector>
# include <map>
# include <csignal>
# include <poll.h>
# include "Client.hpp"

// The engine. Owns the listening socket, the single poll() loop, and every
// connected Client. One thread, non-blocking, no forking.
class Server
{
	private:
		int							_port;
		std::string					_password;
		int							_listenFd;		// the listening socket
		std::vector<struct pollfd>	_pollFds;		// listen fd + all client fds
		std::map<int, Client*>		_clients;		// fd -> Client

		// SIGINT handler flips this so the main loop can exit cleanly.
		static volatile sig_atomic_t	_running;

		// --- setup ---
		void	setupSocket();						// socket/setsockopt/bind/listen/nonblock

		// --- loop steps ---
		void	acceptClient();						// accept + register a new client
		void	handleClientData(int fd);			// recv + drain complete lines
		void	processLine(Client &client, const std::string &line); // milestone-1 stub

		// --- cleanup ---
		void	removeClient(int fd);				// close fd, drop from poll + map
		void	removeFromPoll(int fd);

		// copying a server makes no sense (owns fds + raw pointers)
		Server(const Server &other);
		Server	&operator=(const Server &other);

	public:
		Server(int port, const std::string &password);
		~Server();

		void			run();						// the heart of the project
		static void		handleSignal(int sig);		// SIGINT/SIGTERM -> stop
};

#endif
