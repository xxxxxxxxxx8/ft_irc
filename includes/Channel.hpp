#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <map>
#include <set>

class Client;

// One IRC channel: its members, operators and modes (i, t, k, o, l).
class Channel
{
	public:
		Channel(const std::string &name);
		~Channel();

		const std::string	&getName() const;

		const std::string	&getTopic() const;
		void				setTopic(const std::string &topic);

		const std::string	&getKey() const;
		void				setKey(const std::string &key);

		bool	isInviteOnly() const;
		void	setInviteOnly(bool on);
		bool	isTopicRestricted() const;
		void	setTopicRestricted(bool on);
		// 0 means no user limit.
		size_t	getLimit() const;
		void	setLimit(size_t limit);

		void	addMember(Client *client);
		void	removeMember(int fd);
		bool	isMember(int fd) const;
		bool	isEmpty() const;
		const std::map<int, Client *>	&getMembers() const;

		void	addOperator(int fd);
		void	removeOperator(int fd);
		bool	isOperator(int fd) const;

		void	addInvite(int fd);
		void	removeInvite(int fd);
		bool	isInvited(int fd) const;

		// Current modes as shown in the RPL_CHANNELMODEIS (324) reply.
		std::string	modeString() const;

	private:
		Channel(const Channel &other);
		Channel &operator=(const Channel &other);

		std::string				_name;
		std::string				_topic;
		std::string				_key;
		bool					_inviteOnly;
		bool					_topicRestricted;
		size_t					_limit;
		std::map<int, Client *>	_members;   // key: client socket fd
		std::set<int>			_operators;
		std::set<int>			_invited;
};

#endif
