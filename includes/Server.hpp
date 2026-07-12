#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <csignal>
#include <poll.h>

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

		void	acceptClient();
		void	receiveData(int fd);
		void	removeClient(int fd);

		int							_port;
		std::string					_password;
		int							_listenFd;
		std::vector<struct pollfd>	_pollFds;

		// Set to 1 by signalHandler() so run() can break out of poll() cleanly.
		static volatile sig_atomic_t	_signal;
};

#endif
