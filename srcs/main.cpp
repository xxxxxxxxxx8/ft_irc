#include "Server.hpp"

#include <iostream>
#include <string>
#include <cstdlib>
#include <cctype>

// Validate that the argument is a numeric port in the allowed range.
static bool parsePort(const std::string &str, int &port)
{
	if (str.empty() || str.length() > 5)
		return false;
	for (size_t i = 0; i < str.length(); ++i)
	{
		if (!std::isdigit(static_cast<unsigned char>(str[i])))
			return false;
	}
	port = std::atoi(str.c_str());
	return (port >= 1024 && port <= 65535);
}

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		std::cerr << "Error: usage: ./ircserv <port> <password>" << std::endl;
		return 1;
	}

	int port = 0;
	if (!parsePort(argv[1], port))
	{
		std::cerr << "Error: port must be a number between 1024 and 65535" << std::endl;
		return 1;
	}

	std::string password(argv[2]);
	if (password.empty())
	{
		std::cerr << "Error: password cannot be empty" << std::endl;
		return 1;
	}

	// Any failure during setup/run is reported through exceptions.
	try
	{
		Server server(port, password);
		server.setup();
		server.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
