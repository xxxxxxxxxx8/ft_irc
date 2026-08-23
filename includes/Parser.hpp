#ifndef PARSER_HPP
# define PARSER_HPP

# include <string>
# include <vector>

struct Message
{
	std::string					command;
	std::vector<std::string>	params;
};

void	parseMessage(const std::string &line, Message &msg);

#endif
