#ifndef CHANNEL_HPP
# define CHANNEL_HPP

# include <cstddef>
# include <set>
# include <string>

class Channel
{
	private:
		std::string		_name;
		std::string		_topic;
		std::string		_key;
		size_t			_limit;
		bool			_inviteOnly;
		bool			_topicLocked;
		std::set<int>	_members;
		std::set<int>	_operators;
		std::set<int>	_invited;

	public:
		Channel();
		explicit Channel(const std::string &name);

		const std::string	&getName() const;
		const std::string	&getTopic() const;
		const std::string	&getKey() const;
		size_t				getLimit() const;
		bool				isInviteOnly() const;
		bool				isTopicLocked() const;
		std::string			modeString() const;

		void	setTopic(const std::string &topic);
		void	setKey(const std::string &key);
		void	setLimit(size_t limit);
		void	setInviteOnly(bool val);
		void	setTopicLocked(bool val);

		const std::set<int>	&getMembers() const;
		bool				isMember(int fd) const;
		bool				isOperator(int fd) const;
		bool				isInvited(int fd) const;
		bool				isEmpty() const;

		void	addMember(int fd);
		void	removeMember(int fd);
		void	addOperator(int fd);
		void	removeOperator(int fd);
		void	addInvite(int fd);
};

#endif
