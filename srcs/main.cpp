#include "../includes/Server.hpp"
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

static bool	isValidPort(const std::string &s, int &port)
{
	long	n;

	if (s.empty())
		return (false);
	for (size_t i = 0; i < s.length(); i++)
	{
		if (s[i] < '0' || s[i] > '9')
			return (false);
	}
	n = std::strtol(s.c_str(), NULL, 10);
	if (n < 1 || n > 65535)
		return (false);
	port = static_cast<int>(n);
	return (true);
}

int	main(int ac, char **av)
{
	int	port = 0;

	if (ac != 3)
		return (std::cerr << "Usage: ./ircserv <port> <password>" << std::endl, 1);
	if (!isValidPort(av[1], port))
		return (std::cerr << "Error: port must be between 1 and 65535" << std::endl, 1);
	try
	{
		Server	serv(port, av[2]);

		serv.init();
		serv.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return (1);
	}
	return (0);
}
