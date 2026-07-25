#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "Debug.hpp"

#include <cctype>
#include <ctime>

// Split "<command> <param> ... [:trailing]" into a command and its
// parameters. An optional leading ":<prefix>" from the client is ignored.
static void splitLine(const std::string &line, std::string &command,
	std::vector<std::string> &params)
{
	size_t i = 0;
	size_t len = line.size();

	while (i < len && line[i] == ' ')
		++i;
	if (i < len && line[i] == ':')
		while (i < len && line[i] != ' ')
			++i;
	while (i < len)
	{
		while (i < len && line[i] == ' ')
			++i;
		if (i >= len)
			break;
		if (!command.empty() && line[i] == ':')
		{
			params.push_back(line.substr(i + 1));
			break;
		}
		size_t start = i;
		while (i < len && line[i] != ' ')
			++i;
		if (command.empty())
			command = line.substr(start, i - start);
		else
			params.push_back(line.substr(start, i - start));
	}
	for (size_t j = 0; j < command.size(); ++j)
		command[j] = std::toupper(static_cast<unsigned char>(command[j]));
}

void Server::handleLine(Client *client, const std::string &line)
{
	std::string command;
	std::vector<std::string> params;

	splitLine(line, command, params);
	if (command.empty())
		return;

	// [debug] shows how the raw line was parsed before dispatching
#ifdef DEBUG_MODE
	std::string dbgParams;
	for (size_t i = 0; i < params.size(); ++i)
		dbgParams += " [" + params[i] + "]";
	DBG("dispatch fd " << client->getFd() << ": command=" << command
		<< " params:" << (dbgParams.empty() ? " (none)" : dbgParams));
#endif

	// Capability negotiation is not supported; ignoring it lets modern
	// clients fall back to a plain connection.
	if (command == "CAP" || command == "PONG")
		return;

	if (command == "PASS")
		return cmdPass(client, params);
	if (command == "NICK")
		return cmdNick(client, params);
	if (command == "USER")
		return cmdUser(client, params);
	if (command == "PING")
		return cmdPing(client, params);
	if (command == "QUIT")
		return cmdQuit(client, params);

	if (!client->isRegistered())
		return reply(client, "451", ":You have not registered");

	if (command == "PRIVMSG")
		return cmdPrivmsg(client, params, false);
	if (command == "NOTICE")
		return cmdPrivmsg(client, params, true);
	if (command == "JOIN")
		return cmdJoin(client, params);
	if (command == "PART")
		return cmdPart(client, params);
	if (command == "KICK")
		return cmdKick(client, params);
	if (command == "INVITE")
		return cmdInvite(client, params);
	if (command == "TOPIC")
		return cmdTopic(client, params);
	if (command == "MODE")
		return cmdMode(client, params);

	reply(client, "421", command + " :Unknown command");
}

void Server::cmdPass(Client *client, const std::vector<std::string> &params)
{
	if (client->isRegistered())
		return reply(client, "462", ":You may not reregister");
	if (params.empty())
		return reply(client, "461", "PASS :Not enough parameters");
	client->setPassOk(params[0] == _password);
	// [debug] a wrong password is only punished later, in tryRegister()
	DBG("fd " << client->getFd() << " PASS "
		<< (client->isPassOk() ? "correct" : "WRONG"));
}

static bool isValidNick(const std::string &nick)
{
	const std::string special = "[]\\`_^{|}";

	if (nick.empty() || nick.size() > 9)
		return false;
	for (size_t i = 0; i < nick.size(); ++i)
	{
		char c = nick[i];
		if (std::isalpha(static_cast<unsigned char>(c))
			|| special.find(c) != std::string::npos)
			continue;
		if (i > 0 && (std::isdigit(static_cast<unsigned char>(c)) || c == '-'))
			continue;
		return false;
	}
	return true;
}

void Server::cmdNick(Client *client, const std::vector<std::string> &params)
{
	if (params.empty())
		return reply(client, "431", ":No nickname given");

	const std::string &nick = params[0];
	if (!isValidNick(nick))
		return reply(client, "432", nick + " :Erroneous nickname");

	Client *other = findClientByNick(nick);
	if (other == client)
		return; // same nickname, nothing to do
	if (other != NULL || toLowerStr(nick) == BOT_NICK)
	{
		// [debug] 433: someone (or the bot) already holds this nickname
		DBG("fd " << client->getFd() << " NICK '" << nick
			<< "' refused: already in use");
		return reply(client, "433", nick + " :Nickname is already in use");
	}

	if (client->isRegistered())
	{
		// Announce the change once to everyone sharing a channel, and to
		// the client itself.
		std::string msg = ":" + client->prefix() + " NICK :" + nick;
		std::map<int, Client *> targets;
		targets[client->getFd()] = client;
		for (std::map<std::string, Channel *>::iterator it = _channels.begin();
			 it != _channels.end(); ++it)
		{
			if (!it->second->isMember(client->getFd()))
				continue;
			const std::map<int, Client *> &members = it->second->getMembers();
			targets.insert(members.begin(), members.end());
		}
		for (std::map<int, Client *>::iterator t = targets.begin();
			 t != targets.end(); ++t)
			queueSend(t->second, msg);
	}
	client->setNickname(nick);
	tryRegister(client);
}

void Server::cmdUser(Client *client, const std::vector<std::string> &params)
{
	if (client->isRegistered())
		return reply(client, "462", ":You may not reregister");
	if (params.size() < 4)
		return reply(client, "461", "USER :Not enough parameters");
	client->setUsername(params[0]);
	client->setRealname(params[3]);
	tryRegister(client);
}

// Registration completes once PASS, NICK and USER have all been provided.
void Server::tryRegister(Client *client)
{
	// [debug] registration state machine: needs NICK + USER + correct PASS
	DBG("tryRegister fd " << client->getFd()
		<< ": nick='" << client->getNickname()
		<< "' user='" << client->getUsername()
		<< "' passOk=" << client->isPassOk()
		<< " registered=" << client->isRegistered());
	if (client->isRegistered() || client->getNickname().empty()
		|| client->getUsername().empty())
		return;
	if (!client->isPassOk())
	{
		// [debug] wrong or missing PASS -> 464 then close the connection
		DBG_EVENT("fd " << client->getFd()
			<< " rejected: password incorrect (464)");
		reply(client, "464", ":Password incorrect");
		client->setClosing(true); // disconnected once the reply is flushed
		return;
	}
	client->setRegistered(true);
	DBG_EVENT("fd " << client->getFd() << " REGISTERED as "
		<< client->prefix());
	reply(client, "001", ":Welcome to the " SERVER_NAME " network, "
		+ client->prefix());
}

void Server::cmdPing(Client *client, const std::vector<std::string> &params)
{
	if (params.empty())
		return reply(client, "409", ":No origin specified");
	queueSend(client, ":" SERVER_NAME " PONG " SERVER_NAME " :" + params[0]);
}

void Server::cmdQuit(Client *client, const std::vector<std::string> &params)
{
	std::string reason = params.empty() ? "Client quit" : params[0];
	disconnectClient(client->getFd(), reason);
}

// Shared by PRIVMSG and NOTICE; NOTICE never triggers error replies.
void Server::cmdPrivmsg(Client *client, const std::vector<std::string> &params,
	bool isNotice)
{
	const std::string name = isNotice ? "NOTICE" : "PRIVMSG";

	if (params.empty())
	{
		if (!isNotice)
			reply(client, "411", ":No recipient given (PRIVMSG)");
		return;
	}
	if (params.size() < 2)
	{
		if (!isNotice)
			reply(client, "412", ":No text to send");
		return;
	}
	const std::string &target = params[0];
	const std::string &text = params[1];

	if (target[0] == '#' || target[0] == '&')
	{
		Channel *channel = findChannel(target);
		if (channel == NULL)
		{
			if (!isNotice)
				reply(client, "403", target + " :No such channel");
			return;
		}
		if (!channel->isMember(client->getFd()))
		{
			if (!isNotice)
				reply(client, "404", target + " :Cannot send to channel");
			return;
		}
		broadcast(channel, ":" + client->prefix() + " " + name + " "
			+ channel->getName() + " :" + text, client->getFd());
		return;
	}

	if (toLowerStr(target) == BOT_NICK)
	{
		if (!isNotice)
			botRespond(client, text);
		return;
	}

	Client *dest = findClientByNick(target);
	if (dest == NULL)
	{
		if (!isNotice)
			reply(client, "401", target + " :No such nick/channel");
		return;
	}
	queueSend(dest, ":" + client->prefix() + " " + name + " "
		+ dest->getNickname() + " :" + text);
}

// Bonus: a small bot users can talk to with /msg marvin <command>.
void Server::botRespond(Client *client, const std::string &text)
{
	std::string word = text.substr(0, text.find(' '));
	std::string answer;

	if (toLowerStr(word) == "!help")
		answer = "Available commands: !help, !time, !ping";
	else if (toLowerStr(word) == "!time")
	{
		std::time_t now = std::time(NULL);
		answer = std::ctime(&now);
		answer.erase(answer.size() - 1); // drop the trailing '\n' of ctime()
	}
	else if (toLowerStr(word) == "!ping")
		answer = "pong";
	else
		answer = "Unknown command, try !help";

	queueSend(client, ":" BOT_NICK "!bot@" SERVER_NAME " PRIVMSG "
		+ client->getNickname() + " :" + answer);
}
