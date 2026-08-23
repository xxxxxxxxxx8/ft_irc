#include "../includes/Parser.hpp"
#include <cctype>

static std::string	toUpper(const std::string &s)
{
	std::string	out(s);

	for (size_t i = 0; i < out.size(); i++)
		out[i] = static_cast<char>(std::toupper(
				static_cast<unsigned char>(out[i])));
	return (out);
}

void	parseMessage(const std::string &line, Message &msg)
{
	size_t	i = 0;
	size_t	start;

	msg.prefix.clear();
	msg.command.clear();
	msg.params.clear();
	while (i < line.size() && line[i] == ' ')
		i++;
	if (i < line.size() && line[i] == ':')
	{
		start = ++i;
		while (i < line.size() && line[i] != ' ')
			i++;
		msg.prefix = line.substr(start, i - start);
	}
	while (i < line.size())
	{
		while (i < line.size() && line[i] == ' ')
			i++;
		if (i >= line.size())
			break ;
		if (line[i] == ':' && !msg.command.empty())
		{
			msg.params.push_back(line.substr(i + 1));
			return ;
		}
		start = i;
		while (i < line.size() && line[i] != ' ')
			i++;
		if (msg.command.empty())
			msg.command = toUpper(line.substr(start, i - start));
		else
			msg.params.push_back(line.substr(start, i - start));
	}
}
