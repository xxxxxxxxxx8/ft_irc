NAME     = ircserv

CXX      = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98
INCLUDES = -I includes

SRCS     = srcs/main.cpp \
           srcs/Server.cpp \
           srcs/Client.cpp \
           srcs/Channel.cpp \
           srcs/Parser.cpp \
           srcs/Commands.cpp \
           srcs/ChannelCommands.cpp

HEADERS  = includes/Server.hpp \
           includes/Client.hpp \
           includes/Channel.hpp \
           includes/Parser.hpp

OBJDIR   = obj
OBJS     = $(SRCS:srcs/%.cpp=$(OBJDIR)/%.o)

BONUS_NAME = bot
BONUS_SRCS = bonus/Bot.cpp
BONUS_OBJS = $(BONUS_SRCS:bonus/%.cpp=$(OBJDIR)/bonus_%.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

bonus: $(BONUS_NAME)

$(BONUS_NAME): $(BONUS_OBJS)
	$(CXX) $(CXXFLAGS) $(BONUS_OBJS) -o $(BONUS_NAME)

$(OBJDIR)/%.o: srcs/%.cpp $(HEADERS) | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJDIR)/bonus_%.o: bonus/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR)

fclean: clean
	rm -f $(NAME) $(BONUS_NAME)

re: fclean all

.PHONY: all clean fclean re bonus
