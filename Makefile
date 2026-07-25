NAME		= ircserv

CXX			= c++
CXXFLAGS	= -Wall -Wextra -Werror -std=c++98
INCLUDES	= -I includes

SRCDIR		= srcs
OBJDIR		= objs

SRCS		= main.cpp \
			  Server.cpp \
			  Client.cpp

OBJS		= $(addprefix $(OBJDIR)/, $(SRCS:.cpp=.o))

all: $(NAME)

# Debug build: enables the DBG* macros of includes/Debug.hpp (traces on
# stderr) and embeds debug symbols for gdb/valgrind. Always rebuilds from
# scratch so debug and release objects are never mixed.
# Go back to a normal build with: make re
debug: CXXFLAGS += -DDEBUG_MODE -g3
debug: fclean $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(OBJDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(OBJDIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re debug
