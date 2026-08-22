#include "../includes/Channel.hpp"

Channel::Channel() {}

Channel::Channel(const std::string &name) : _name(name) {}

Channel::Channel(const Channel &src)
	: _name(src._name), _topic(src._topic), _key(src._key) {}

Channel &Channel::operator=(const Channel &rhs)
{
	if (this != &rhs)
	{
		_name = rhs._name;
		_topic = rhs._topic;
		_key = rhs._key;
	}
	return (*this);
}

Channel::~Channel() {}

const std::string	&Channel::getName() const { return (_name); }

const std::string	&Channel::getTopic() const { return (_topic); }

void	Channel::setTopic(const std::string &topic) { _topic = topic; }
