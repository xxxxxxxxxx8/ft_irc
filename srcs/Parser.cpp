#include "../includes/Parser.hpp"
#include <cstddef>

void	parseMessage(const std::string &line, Message &msg)
{
	size_t	i = 0;
	size_t	start;

	while (i < line.size())
	{
		while (i < line.size() && line[i] == ' ')
			i++;
		if (i >= line.size())
			break ;
		if (line[i] == ':' && !msg.command.empty())
		{
			msg.params.push_back(line.substr(i + 1));
			break ;
		}
		start = i;
		while (i < line.size() && line[i] != ' ')
			i++;
		if (msg.command.empty())
			msg.command = line.substr(start, i - start);
		else
			msg.params.push_back(line.substr(start, i - start));
	}
	for (size_t j = 0; j < msg.command.size(); j++)
	{
		if (msg.command[j] >= 'a' && msg.command[j] <= 'z')
			msg.command[j] = static_cast<char>(msg.command[j] - ('a' - 'A'));
	}
}
