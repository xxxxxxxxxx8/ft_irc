#ifndef CHANNEL_HPP
# define CHANNEL_HPP

# include "Client.hpp"
# include <string>
# include <vector>
# include <map>

class Channel
{
	private:
		std::string	_name;
		std::string	_topic;
		std::string	_key;

	public:
		Channel();
		Channel(const std::string &name);
		Channel(const Channel &src);
		Channel &operator=(const Channel &rhs);
		~Channel();

		const std::string	&getName() const;
		const std::string	&getTopic() const;
		void				setTopic(const std::string &topic);
};

#endif
