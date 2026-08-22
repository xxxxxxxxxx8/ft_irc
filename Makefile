NAME     = ircserv

CXX      = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98
INCLUDES = -I includes

SRCS     = srcs/main.cpp \
           srcs/Server.cpp

OBJDIR   = obj
OBJS     = $(SRCS:srcs/%.cpp=$(OBJDIR)/%.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(OBJDIR)/%.o: srcs/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
