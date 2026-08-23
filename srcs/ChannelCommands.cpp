#include "../includes/Server.hpp"
#include <sstream>

static bool	isChannelName(const std::string &name)
{
	return (!name.empty() && (name[0] == '#' || name[0] == '&'));
}

static size_t	parseLimit(const std::string &s)
{
	std::istringstream	in(s);
	long				value = 0;

	in >> value;
	if (in.fail() || value < 0)
		return (0);
	return (static_cast<size_t>(value));
}

void	Server::sendJoinInfo(Client &client, Channel &channel)
{
	const std::set<int>	&members = channel.getMembers();
	std::string			names;

	broadcast(channel, ":" + client.prefix() + " JOIN " + channel.getName(),
		-1);
	if (channel.getTopic().empty())
		reply(client, "331", channel.getName() + " :No topic is set");
	else
		reply(client, "332", channel.getName() + " :" + channel.getTopic());
	for (std::set<int>::const_iterator it = members.begin();
		it != members.end(); ++it)
	{
		std::map<int, Client>::iterator	c = _clients.find(*it);

		if (c == _clients.end())
			continue ;
		if (!names.empty())
			names += " ";
		if (channel.isOperator(*it))
			names += "@";
		names += c->second.getNickname();
	}
	reply(client, "353", "= " + channel.getName() + " :" + names);
	reply(client, "366", channel.getName() + " :End of /NAMES list");
}

void	Server::cmdJoin(Client &client, const std::vector<std::string> &params)
{
	if (params.empty() || params[0].empty())
		return (reply(client, "461", "JOIN :Not enough parameters"));

	const std::string	&name = params[0];

	if (!isChannelName(name))
		return (reply(client, "403", name + " :No such channel"));

	Channel	*channel = findChannel(name);

	if (channel == NULL)
	{
		Channel	created(name);

		created.addMember(client.getFd());
		created.addOperator(client.getFd());
		channel = &_channels.insert(
				std::make_pair(toLower(name), created)).first->second;
		return (sendJoinInfo(client, *channel));
	}
	if (channel->isMember(client.getFd()))
		return ;
	if (channel->isInviteOnly() && !channel->isInvited(client.getFd()))
		return (reply(client, "473", channel->getName()
			+ " :Cannot join channel (+i)"));
	if (!channel->getKey().empty()
		&& (params.size() < 2 || params[1] != channel->getKey()))
		return (reply(client, "475", channel->getName()
			+ " :Cannot join channel (+k)"));
	if (channel->getLimit() > 0
		&& channel->getMembers().size() >= channel->getLimit())
		return (reply(client, "471", channel->getName()
			+ " :Cannot join channel (+l)"));
	channel->addMember(client.getFd());
	sendJoinInfo(client, *channel);
}

void	Server::cmdKick(Client &client, const std::vector<std::string> &params)
{
	if (params.size() < 2)
		return (reply(client, "461", "KICK :Not enough parameters"));

	Channel	*channel = findChannel(params[0]);

	if (channel == NULL)
		return (reply(client, "403", params[0] + " :No such channel"));
	if (!channel->isMember(client.getFd()))
		return (reply(client, "442", channel->getName()
			+ " :You're not on that channel"));
	if (!channel->isOperator(client.getFd()))
		return (reply(client, "482", channel->getName()
			+ " :You're not channel operator"));

	Client	*target = findClientByNick(params[1]);

	if (target == NULL || !channel->isMember(target->getFd()))
		return (reply(client, "441", params[1] + " " + channel->getName()
			+ " :They aren't on that channel"));

	std::string	reason = params.size() > 2 ? params[2] : client.getNickname();
	std::string	key = toLower(channel->getName());

	broadcast(*channel, ":" + client.prefix() + " KICK " + channel->getName()
		+ " " + target->getNickname() + " :" + reason, -1);
	channel->removeMember(target->getFd());
	if (channel->isEmpty())
		_channels.erase(key);
}

void	Server::cmdInvite(Client &client,
	const std::vector<std::string> &params)
{
	if (params.size() < 2)
		return (reply(client, "461", "INVITE :Not enough parameters"));

	Channel	*channel = findChannel(params[1]);

	if (channel == NULL)
		return (reply(client, "403", params[1] + " :No such channel"));
	if (!channel->isMember(client.getFd()))
		return (reply(client, "442", channel->getName()
			+ " :You're not on that channel"));
	if (!channel->isOperator(client.getFd()))
		return (reply(client, "482", channel->getName()
			+ " :You're not channel operator"));

	Client	*target = findClientByNick(params[0]);

	if (target == NULL)
		return (reply(client, "401", params[0] + " :No such nick/channel"));
	if (channel->isMember(target->getFd()))
		return (reply(client, "443", target->getNickname() + " "
			+ channel->getName() + " :is already on channel"));

	channel->addInvite(target->getFd());
	reply(client, "341", target->getNickname() + " " + channel->getName());
	target->queue(":" + client.prefix() + " INVITE " + target->getNickname()
		+ " :" + channel->getName());
}

void	Server::cmdTopic(Client &client, const std::vector<std::string> &params)
{
	if (params.empty())
		return (reply(client, "461", "TOPIC :Not enough parameters"));

	Channel	*channel = findChannel(params[0]);

	if (channel == NULL)
		return (reply(client, "403", params[0] + " :No such channel"));
	if (!channel->isMember(client.getFd()))
		return (reply(client, "442", channel->getName()
			+ " :You're not on that channel"));
	if (params.size() < 2)
	{
		if (channel->getTopic().empty())
			return (reply(client, "331", channel->getName()
				+ " :No topic is set"));
		return (reply(client, "332", channel->getName() + " :"
			+ channel->getTopic()));
	}
	if (channel->isTopicLocked() && !channel->isOperator(client.getFd()))
		return (reply(client, "482", channel->getName()
			+ " :You're not channel operator"));
	channel->setTopic(params[1]);
	broadcast(*channel, ":" + client.prefix() + " TOPIC " + channel->getName()
		+ " :" + params[1], -1);
}

void	Server::applyMode(Client &client, Channel &channel, char mode,
	bool adding, const std::vector<std::string> &params, size_t &arg)
{
	std::string	value;

	if (mode == 'o' || ((mode == 'k' || mode == 'l') && adding))
	{
		if (arg >= params.size())
			return (reply(client, "461", "MODE :Not enough parameters"));
		value = params[arg++];
	}
	if (mode == 'i')
		channel.setInviteOnly(adding);
	else if (mode == 't')
		channel.setTopicLocked(adding);
	else if (mode == 'k')
		channel.setKey(adding ? value : "");
	else if (mode == 'l')
		channel.setLimit(adding ? parseLimit(value) : 0);
	else if (mode == 'o')
	{
		Client	*target = findClientByNick(value);

		if (target == NULL || !channel.isMember(target->getFd()))
			return (reply(client, "441", value + " " + channel.getName()
				+ " :They aren't on that channel"));
		if (adding)
			channel.addOperator(target->getFd());
		else
			channel.removeOperator(target->getFd());
	}
	else
		return (reply(client, "472", std::string(1, mode)
			+ " :is unknown mode char to me"));

	std::string	msg = ":" + client.prefix() + " MODE " + channel.getName()
		+ " " + (adding ? "+" : "-") + mode;

	if (!value.empty())
		msg += " " + value;
	broadcast(channel, msg, -1);
}

void	Server::cmdMode(Client &client, const std::vector<std::string> &params)
{
	if (params.empty())
		return (reply(client, "461", "MODE :Not enough parameters"));
	if (!isChannelName(params[0]))
		return ;

	Channel	*channel = findChannel(params[0]);

	if (channel == NULL)
		return (reply(client, "403", params[0] + " :No such channel"));
	if (params.size() < 2)
		return (reply(client, "324", channel->getName() + " "
			+ channel->modeString()));
	if (!channel->isMember(client.getFd()))
		return (reply(client, "442", channel->getName()
			+ " :You're not on that channel"));
	if (!channel->isOperator(client.getFd()))
		return (reply(client, "482", channel->getName()
			+ " :You're not channel operator"));

	bool	adding = true;
	size_t	arg = 2;

	for (size_t i = 0; i < params[1].size(); i++)
	{
		if (params[1][i] == '+' || params[1][i] == '-')
			adding = (params[1][i] == '+');
		else
			applyMode(client, *channel, params[1][i], adding, params, arg);
	}
}
