#include "../includes/Server.hpp"
#include "../includes/Parser.hpp"

static bool	isValidNick(const std::string &nick)
{
	if (nick.empty())
		return (false);
	if (nick.find_first_of(" !@") != std::string::npos)
		return (false);
	return (nick[0] != ':' && nick[0] != '#' && nick[0] != '&');
}

void	Server::handleLine(Client &client, const std::string &line)
{
	Message	msg;

	parseMessage(line, msg);
	if (msg.command.empty())
		return ;
	if (msg.command == "PASS")
		cmdPass(client, msg.params);
	else if (msg.command == "NICK")
		cmdNick(client, msg.params);
	else if (msg.command == "USER")
		cmdUser(client, msg.params);
	else if (!client.isRegistered())
		reply(client, "451", ":You have not registered");
	else if (msg.command == "PRIVMSG")
		cmdPrivmsg(client, msg.params);
	else if (msg.command == "JOIN")
		cmdJoin(client, msg.params);
	else if (msg.command == "KICK")
		cmdKick(client, msg.params);
	else if (msg.command == "INVITE")
		cmdInvite(client, msg.params);
	else if (msg.command == "TOPIC")
		cmdTopic(client, msg.params);
	else if (msg.command == "MODE")
		cmdMode(client, msg.params);
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

	if (other != NULL && other != &client)
		return (reply(client, "433", nick + " :Nickname is already in use"));
	if (nick == client.getNickname())
		return ;
	if (client.isRegistered())
	{
		std::string	msg = ":" + client.prefix() + " NICK :" + nick;

		client.queue(msg);
		broadcastToPeers(client, msg);
	}
	client.setNickname(nick);
	tryRegister(client);
}

void	Server::cmdUser(Client &client, const std::vector<std::string> &params)
{
	if (client.isRegistered())
		return (reply(client, "462", ":You may not reregister"));
	if (params.size() < 4 || params[0].empty())
		return (reply(client, "461", "USER :Not enough parameters"));
	client.setUsername(params[0]);
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
	reply(client, "001", ":Welcome to the " SERVER_NAME " Network, "
		+ client.prefix());
}


void	Server::cmdPrivmsg(Client &client,
	const std::vector<std::string> &params)
{
	if (params.empty())
		return (reply(client, "411", ":No recipient given (PRIVMSG)"));
	if (params.size() < 2 || params[1].empty())
		return (reply(client, "412", ":No text to send"));

	const std::string	&target = params[0];

	if (target[0] == '#' || target[0] == '&')
	{
		Channel	*channel = findChannel(target);

		if (channel == NULL)
			return (reply(client, "403", target + " :No such channel"));
		if (!channel->isMember(client.getFd()))
			return (reply(client, "404", channel->getName()
				+ " :Cannot send to channel"));
		return (broadcast(*channel, ":" + client.prefix() + " PRIVMSG "
			+ channel->getName() + " :" + params[1], client.getFd()));
	}

	Client	*dest = findClientByNick(target);

	if (dest == NULL)
		return (reply(client, "401", target + " :No such nick/channel"));
	dest->queue(":" + client.prefix() + " PRIVMSG " + dest->getNickname()
		+ " :" + params[1]);
}
