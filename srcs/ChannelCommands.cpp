#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "Debug.hpp"

#include <cstdlib>

static bool isChannelName(const std::string &name)
{
	return !name.empty() && (name[0] == '#' || name[0] == '&');
}

static std::vector<std::string> splitOnComma(const std::string &s)
{
	std::vector<std::string> out;
	size_t start = 0;

	while (start <= s.size())
	{
		size_t comma = s.find(',', start);
		if (comma == std::string::npos)
		{
			out.push_back(s.substr(start));
			break;
		}
		out.push_back(s.substr(start, comma - start));
		start = comma + 1;
	}
	return out;
}

// RPL_NAMREPLY (353) + RPL_ENDOFNAMES (366), sent to a client joining.
void Server::sendNames(Client *client, Channel *channel)
{
	std::string names;
	const std::map<int, Client *> &members = channel->getMembers();

	for (std::map<int, Client *>::const_iterator it = members.begin();
		 it != members.end(); ++it)
	{
		if (!names.empty())
			names += " ";
		if (channel->isOperator(it->first))
			names += "@";
		names += it->second->getNickname();
	}
	reply(client, "353", "= " + channel->getName() + " :" + names);
	reply(client, "366", channel->getName() + " :End of /NAMES list");
}

void Server::cmdJoin(Client *client, const std::vector<std::string> &params)
{
	if (params.empty())
		return reply(client, "461", "JOIN :Not enough parameters");

	std::vector<std::string> names = splitOnComma(params[0]);
	std::vector<std::string> keys;
	if (params.size() > 1)
		keys = splitOnComma(params[1]);

	for (size_t i = 0; i < names.size(); ++i)
	{
		const std::string &name = names[i];
		std::string key = i < keys.size() ? keys[i] : "";

		if (!isChannelName(name))
		{
			reply(client, "403", name + " :No such channel");
			continue;
		}
		Channel *channel = findChannel(name);
		bool created = channel == NULL;
		if (created)
		{
			channel = new Channel(name);
			_channels[toLowerStr(name)] = channel;
			DBG_EVENT("channel " << name << " created by "
				<< client->getNickname());
		}
		if (channel->isMember(client->getFd()))
			continue;
		if (channel->isInviteOnly() && !channel->isInvited(client->getFd()))
		{
			// [debug] +i mode: only invited clients may enter
			DBG("JOIN " << name << " refused for " << client->getNickname()
				<< ": invite-only (+i), no invite");
			reply(client, "473", name + " :Cannot join channel (+i)");
			continue;
		}
		if (!channel->getKey().empty() && key != channel->getKey())
		{
			// [debug] +k mode: the key given with JOIN does not match
			DBG("JOIN " << name << " refused for " << client->getNickname()
				<< ": wrong channel key (+k)");
			reply(client, "475", name + " :Cannot join channel (+k)");
			continue;
		}
		if (channel->getLimit() > 0
			&& channel->getMembers().size() >= channel->getLimit())
		{
			// [debug] +l mode: the channel is full
			DBG("JOIN " << name << " refused for " << client->getNickname()
				<< ": limit reached (" << channel->getLimit() << ") (+l)");
			reply(client, "471", name + " :Cannot join channel (+l)");
			continue;
		}

		channel->addMember(client);
		channel->removeInvite(client->getFd());
		// The creator of a channel becomes its first operator.
		if (created)
			channel->addOperator(client->getFd());
		DBG_EVENT(client->getNickname() << " joined " << channel->getName()
			<< (created ? " as operator (creator)" : "") << ", members: "
			<< channel->getMembers().size());

		broadcast(channel, ":" + client->prefix() + " JOIN :"
			+ channel->getName(), -1);
		if (!channel->getTopic().empty())
			reply(client, "332", channel->getName() + " :"
				+ channel->getTopic());
		sendNames(client, channel);
	}
}

void Server::cmdPart(Client *client, const std::vector<std::string> &params)
{
	if (params.empty())
		return reply(client, "461", "PART :Not enough parameters");

	Channel *channel = findChannel(params[0]);
	if (channel == NULL)
		return reply(client, "403", params[0] + " :No such channel");
	if (!channel->isMember(client->getFd()))
		return reply(client, "442", params[0]
			+ " :You're not on that channel");

	std::string msg = ":" + client->prefix() + " PART " + channel->getName();
	if (params.size() > 1)
		msg += " :" + params[1];
	broadcast(channel, msg, -1);
	channel->removeMember(client->getFd());
	dropChannelIfEmpty(channel);
}

void Server::cmdKick(Client *client, const std::vector<std::string> &params)
{
	if (params.size() < 2)
		return reply(client, "461", "KICK :Not enough parameters");

	Channel *channel = findChannel(params[0]);
	if (channel == NULL)
		return reply(client, "403", params[0] + " :No such channel");
	if (!channel->isMember(client->getFd()))
		return reply(client, "442", params[0]
			+ " :You're not on that channel");
	if (!channel->isOperator(client->getFd()))
		return reply(client, "482", channel->getName()
			+ " :You're not channel operator");

	Client *target = findClientByNick(params[1]);
	if (target == NULL || !channel->isMember(target->getFd()))
		return reply(client, "441", params[1] + " " + channel->getName()
			+ " :They aren't on that channel");

	std::string comment = params.size() > 2 ? params[2]
											: client->getNickname();
	DBG_EVENT(client->getNickname() << " KICKed " << target->getNickname()
		<< " from " << channel->getName() << " (" << comment << ")");
	broadcast(channel, ":" + client->prefix() + " KICK " + channel->getName()
		+ " " + target->getNickname() + " :" + comment, -1);
	channel->removeMember(target->getFd());
	dropChannelIfEmpty(channel);
}

void Server::cmdInvite(Client *client, const std::vector<std::string> &params)
{
	if (params.size() < 2)
		return reply(client, "461", "INVITE :Not enough parameters");

	Client *target = findClientByNick(params[0]);
	if (target == NULL)
		return reply(client, "401", params[0] + " :No such nick/channel");

	Channel *channel = findChannel(params[1]);
	if (channel == NULL)
		return reply(client, "403", params[1] + " :No such channel");
	if (!channel->isMember(client->getFd()))
		return reply(client, "442", params[1]
			+ " :You're not on that channel");
	if (channel->isInviteOnly() && !channel->isOperator(client->getFd()))
		return reply(client, "482", channel->getName()
			+ " :You're not channel operator");
	if (channel->isMember(target->getFd()))
		return reply(client, "443", target->getNickname() + " "
			+ channel->getName() + " :is already on channel");

	channel->addInvite(target->getFd());
	reply(client, "341", target->getNickname() + " " + channel->getName());
	queueSend(target, ":" + client->prefix() + " INVITE "
		+ target->getNickname() + " :" + channel->getName());
}

void Server::cmdTopic(Client *client, const std::vector<std::string> &params)
{
	if (params.empty())
		return reply(client, "461", "TOPIC :Not enough parameters");

	Channel *channel = findChannel(params[0]);
	if (channel == NULL)
		return reply(client, "403", params[0] + " :No such channel");
	if (!channel->isMember(client->getFd()))
		return reply(client, "442", params[0]
			+ " :You're not on that channel");

	// Without a second parameter, TOPIC only views the current topic.
	if (params.size() == 1)
	{
		if (channel->getTopic().empty())
			return reply(client, "331", channel->getName()
				+ " :No topic is set");
		return reply(client, "332", channel->getName() + " :"
			+ channel->getTopic());
	}

	if (channel->isTopicRestricted() && !channel->isOperator(client->getFd()))
		return reply(client, "482", channel->getName()
			+ " :You're not channel operator");

	channel->setTopic(params[1]);
	broadcast(channel, ":" + client->prefix() + " TOPIC " + channel->getName()
		+ " :" + channel->getTopic(), -1);
}

void Server::cmdMode(Client *client, const std::vector<std::string> &params)
{
	if (params.empty())
		return reply(client, "461", "MODE :Not enough parameters");

	// User modes are out of scope: silently ignore them so that clients
	// setting their own mode at connection do not get errors.
	if (!isChannelName(params[0]))
		return;

	Channel *channel = findChannel(params[0]);
	if (channel == NULL)
		return reply(client, "403", params[0] + " :No such channel");

	if (params.size() == 1)
		return reply(client, "324", channel->getName() + " "
			+ channel->modeString());

	if (!channel->isOperator(client->getFd()))
		return reply(client, "482", channel->getName()
			+ " :You're not channel operator");

	// Apply the mode string, collecting what actually changed so the
	// change can be echoed to the channel in a single MODE message.
	const std::string &modes = params[1];
	size_t argIndex = 2;
	bool adding = true;
	char lastSign = 0;
	std::string appliedModes;
	std::string appliedArgs;

	for (size_t i = 0; i < modes.size(); ++i)
	{
		char c = modes[i];
		if (c == '+' || c == '-')
		{
			adding = (c == '+');
			continue;
		}

		std::string arg;
		bool needsArg = (c == 'k' && adding) || (c == 'l' && adding)
			|| c == 'o';
		if (needsArg)
		{
			if (argIndex >= params.size())
			{
				reply(client, "461", "MODE :Not enough parameters");
				continue;
			}
			arg = params[argIndex++];
		}

		if (c == 'i')
			channel->setInviteOnly(adding);
		else if (c == 't')
			channel->setTopicRestricted(adding);
		else if (c == 'k')
		{
			channel->setKey(adding ? arg : "");
			if (!adding)
				arg = "*";
		}
		else if (c == 'l')
		{
			if (adding)
			{
				int limit = std::atoi(arg.c_str());
				if (limit <= 0)
					continue;
				channel->setLimit(limit);
			}
			else
				channel->setLimit(0);
		}
		else if (c == 'o')
		{
			Client *target = findClientByNick(arg);
			if (target == NULL || !channel->isMember(target->getFd()))
			{
				reply(client, "441", arg + " " + channel->getName()
					+ " :They aren't on that channel");
				continue;
			}
			if (adding)
				channel->addOperator(target->getFd());
			else
				channel->removeOperator(target->getFd());
			arg = target->getNickname();
		}
		else
		{
			reply(client, "472", std::string(1, c)
				+ " :is unknown mode char to me");
			continue;
		}

		char sign = adding ? '+' : '-';
		if (sign != lastSign)
		{
			appliedModes += sign;
			lastSign = sign;
		}
		appliedModes += c;
		if (!arg.empty())
			appliedArgs += " " + arg;
	}

	if (!appliedModes.empty())
	{
		// [debug] only the modes that really changed are echoed
		DBG_EVENT("MODE " << channel->getName() << " " << appliedModes
			<< appliedArgs << " applied by " << client->getNickname()
			<< " -> now: " << channel->modeString());
		broadcast(channel, ":" + client->prefix() + " MODE "
			+ channel->getName() + " " + appliedModes + appliedArgs, -1);
	}
}
