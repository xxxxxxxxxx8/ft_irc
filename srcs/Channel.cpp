#include "Channel.hpp"
#include "Client.hpp"

#include <sstream>

Channel::Channel(const std::string &name)
	: _name(name), _inviteOnly(false), _topicRestricted(false), _limit(0)
{
}

Channel::~Channel()
{
}

const std::string &Channel::getName() const
{
	return _name;
}

const std::string &Channel::getTopic() const
{
	return _topic;
}

void Channel::setTopic(const std::string &topic)
{
	_topic = topic;
}

const std::string &Channel::getKey() const
{
	return _key;
}

void Channel::setKey(const std::string &key)
{
	_key = key;
}

bool Channel::isInviteOnly() const
{
	return _inviteOnly;
}

void Channel::setInviteOnly(bool on)
{
	_inviteOnly = on;
}

bool Channel::isTopicRestricted() const
{
	return _topicRestricted;
}

void Channel::setTopicRestricted(bool on)
{
	_topicRestricted = on;
}

size_t Channel::getLimit() const
{
	return _limit;
}

void Channel::setLimit(size_t limit)
{
	_limit = limit;
}

void Channel::addMember(Client *client)
{
	_members[client->getFd()] = client;
}

void Channel::removeMember(int fd)
{
	_members.erase(fd);
	_operators.erase(fd);
	_invited.erase(fd);
}

bool Channel::isMember(int fd) const
{
	return _members.find(fd) != _members.end();
}

bool Channel::isEmpty() const
{
	return _members.empty();
}

const std::map<int, Client *> &Channel::getMembers() const
{
	return _members;
}

void Channel::addOperator(int fd)
{
	_operators.insert(fd);
}

void Channel::removeOperator(int fd)
{
	_operators.erase(fd);
}

bool Channel::isOperator(int fd) const
{
	return _operators.find(fd) != _operators.end();
}

void Channel::addInvite(int fd)
{
	_invited.insert(fd);
}

void Channel::removeInvite(int fd)
{
	_invited.erase(fd);
}

bool Channel::isInvited(int fd) const
{
	return _invited.find(fd) != _invited.end();
}

std::string Channel::modeString() const
{
	std::string modes = "+";
	std::string args;

	if (_inviteOnly)
		modes += "i";
	if (_topicRestricted)
		modes += "t";
	if (!_key.empty())
	{
		modes += "k";
		args += " " + _key;
	}
	if (_limit > 0)
	{
		std::ostringstream oss;
		oss << _limit;
		modes += "l";
		args += " " + oss.str();
	}
	return modes + args;
}
