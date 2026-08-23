#include "../includes/Server.hpp"
#include <iostream>
#include <cstdlib>
#include <cctype>
#include <string>

static bool	is_valid_port(const std::string &s, int &port)
{
	if (s.empty() || s.length() > 5)
		return (false);
	for (size_t i = 0; i < s.length(); i++)
	{
		if (!std::isdigit(static_cast<unsigned char>(s[i])))
			return (false);
	}
	long n = std::strtol(s.c_str(), NULL, 10);
	if (n < 1 || n > 65535)
		return (false);
	port = static_cast<int>(n);
	return (true);
}

int	main(int ac, char **av)
{
	if (ac != 3)
	{
		std::cerr << "Usage: ./ircserv <port> <password>" << std::endl;
		return (1);
	}

	int	port = 0;
	if (!is_valid_port(av[1], port))
	{
		std::cerr << "Error: port must be between 1 and 65535" << std::endl;
		return (1);
	}

	std::string	pass(av[2]);
	if (pass.empty())
	{
		std::cerr << "Error: password can't be empty" << std::endl;
		return (1);
	}

	try
	{
		Server	serv(port, pass);
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
