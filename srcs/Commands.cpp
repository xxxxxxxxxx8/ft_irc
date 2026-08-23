#include "../includes/Server.hpp"
#include "../includes/Parser.hpp"
#include <iostream>
#include <cctype>

static std::string	toLower(const std::string &s)
{
	std::string	out(s);

	for (size_t i = 0; i < out.size(); i++)
		out[i] = static_cast<char>(std::tolower(
				static_cast<unsigned char>(out[i])));
	return (out);
}

static bool	isValidNick(const std::string &nick)
{
	if (nick.empty())
		return (false);
	if (nick.find_first_of(" ,!@") != std::string::npos)
		return (false);
	return (nick[0] != ':' && nick[0] != '#' && nick[0] != '&');
}

Client	*Server::findClientByNick(const std::string &nick)
{
	std::string	target = toLower(nick);

	if (target.empty())
		return (NULL);
	for (std::map<int, Client>::iterator it = _clients.begin();
		it != _clients.end(); ++it)
	{
		if (toLower(it->second.getNickname()) == target)
			return (&it->second);
	}
	return (NULL);
}

void	Server::reply(Client &client, const std::string &code,
	const std::string &text)
{
	std::string	nick = client.getNickname();

	if (nick.empty())
		nick = "*";

	sendReply(client.getFd(), ":" SERVER_NAME " " + code + " " + nick + " "
		+ text + "\r\n");
}

void	Server::handleLine(Client &client, const std::string &line)
{
	Message	msg;

	parseMessage(line, msg);
	if (msg.command.empty())
		return ;
	std::cout << "[fd " << client.getFd() << "] " << line << std::endl;

	if (msg.command == "CAP" || msg.command == "PONG")
		return ;
	if (msg.command == "PASS")
		cmdPass(client, msg.params);
	else if (msg.command == "NICK")
		cmdNick(client, msg.params);
	else if (msg.command == "USER")
		cmdUser(client, msg.params);
	else if (msg.command == "PING")
		cmdPing(client, msg.params);
	else if (msg.command == "QUIT")
		cmdQuit(client, msg.params);
	else if (!client.isRegistered())
		reply(client, "451", ":You have not registered");
	else if (msg.command == "PRIVMSG")
		cmdPrivmsg(client, msg.params);
	else
		reply(client, "421", msg.command + " :Unknown command");
}

void	Server::cmdPass(Client &client, const std::vector<std::string> &params)
{
	if (client.isRegistered())
		return (reply(client, "462", ":You may not reregister"));
	if (params.empty())
		return (reply(client, "461", "PASS :Not enough parameters"));
	client.setPassOk(params[0] == _pass);
}

void	Server::cmdNick(Client &client, const std::vector<std::string> &params)
{
	if (params.empty() || params[0].empty())
		return (reply(client, "431", ":No nickname given"));

	const std::string	&nick = params[0];

	if (!isValidNick(nick))
		return (reply(client, "432", nick + " :Erroneous nickname"));

	Client	*other = findClientByNick(nick);

	if (other == &client)
		return ;
	if (other != NULL)
		return (reply(client, "433", nick + " :Nickname is already in use"));
	if (client.isRegistered())
		sendReply(client.getFd(), ":" + client.prefix() + " NICK :" + nick
			+ "\r\n");
	client.setNickname(nick);
	tryRegister(client);
}

void	Server::cmdUser(Client &client, const std::vector<std::string> &params)
{
	if (client.isRegistered())
		return (reply(client, "462", ":You may not reregister"));
	if (params.size() < 4)
		return (reply(client, "461", "USER :Not enough parameters"));
	client.setUsername(params[0]);
	client.setRealname(params[3]);
	tryRegister(client);
}

void	Server::tryRegister(Client &client)
{
	if (client.isRegistered() || client.getNickname().empty()
		|| client.getUsername().empty())
		return ;
	if (!client.isPassOk())
	{
		reply(client, "464", ":Password incorrect");
		client.setClosing(true);
		return ;
	}
	client.setRegistered(true);
	std::cout << "fd " << client.getFd() << " registered as "
		<< client.prefix() << std::endl;
	reply(client, "001", ":Welcome to the " SERVER_NAME " Network, "
		+ client.prefix());
}

void	Server::cmdPing(Client &client, const std::vector<std::string> &params)
{
	if (params.empty())
		return (reply(client, "409", ":No origin specified"));
	sendReply(client.getFd(), ":" SERVER_NAME " PONG " SERVER_NAME " :"
		+ params[0] + "\r\n");
}

void	Server::cmdQuit(Client &client, const std::vector<std::string> &params)
{
	std::string	reason = params.empty() ? "Client quit" : params[0];

	sendReply(client.getFd(), "ERROR :Closing link (" + reason + ")\r\n");
	client.setClosing(true);
}

void	Server::cmdPrivmsg(Client &client,
	const std::vector<std::string> &params)
{
	if (params.empty())
		return (reply(client, "411", ":No recipient given (PRIVMSG)"));
	if (params.size() < 2 || params[1].empty())
		return (reply(client, "412", ":No text to send"));

	Client	*target = findClientByNick(params[0]);

	if (target == NULL)
		return (reply(client, "401", params[0] + " :No such nick/channel"));
	sendReply(target->getFd(), ":" + client.prefix() + " PRIVMSG "
		+ target->getNickname() + " :" + params[1] + "\r\n");
}
