#include "../includes/Channel.hpp"

Channel::Channel()
	: _limit(0), _inviteOnly(false), _topicLocked(false) {}

Channel::Channel(const std::string &name)
	: _name(name), _limit(0), _inviteOnly(false), _topicLocked(false) {}

const std::string	&Channel::getName() const { return (_name); }

const std::string	&Channel::getTopic() const { return (_topic); }

const std::string	&Channel::getKey() const { return (_key); }

size_t	Channel::getLimit() const { return (_limit); }

bool	Channel::isInviteOnly() const { return (_inviteOnly); }

bool	Channel::isTopicLocked() const { return (_topicLocked); }

void	Channel::setTopic(const std::string &topic) { _topic = topic; }

void	Channel::setKey(const std::string &key) { _key = key; }

void	Channel::setLimit(size_t limit) { _limit = limit; }

void	Channel::setInviteOnly(bool val) { _inviteOnly = val; }

void	Channel::setTopicLocked(bool val) { _topicLocked = val; }

const std::set<int>	&Channel::getMembers() const { return (_members); }

bool	Channel::isMember(int fd) const
{
	return (_members.find(fd) != _members.end());
}

bool	Channel::isOperator(int fd) const
{
	return (_operators.find(fd) != _operators.end());
}

bool	Channel::isInvited(int fd) const
{
	return (_invited.find(fd) != _invited.end());
}

bool	Channel::isEmpty() const { return (_members.empty()); }

void	Channel::addMember(int fd)
{
	_members.insert(fd);
	_invited.erase(fd);
}

void	Channel::removeMember(int fd)
{
	_members.erase(fd);
	_operators.erase(fd);
	_invited.erase(fd);
}

void	Channel::addOperator(int fd) { _operators.insert(fd); }

void	Channel::removeOperator(int fd) { _operators.erase(fd); }

void	Channel::addInvite(int fd) { _invited.insert(fd); }
