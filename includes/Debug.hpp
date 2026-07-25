#ifndef DEBUG_HPP
#define DEBUG_HPP

/*
** ============================== DEBUG SYSTEM ===============================
**
** How to use it:
**   make        -> normal build: every DBG* macro compiles to NOTHING
**                  (zero runtime cost, no trace output).
**   make debug  -> debug build (-DDEBUG_MODE -g3): every DBG* macro prints
**                  a colored trace on stderr, so you can watch the server
**                  work in real time while clients stay on stdout.
**
** The macros (all stream-style, so you can chain values with <<):
**   DBG_FUNC()          -> prints "--> functionName()" when a function starts.
**                          Put it at the top of a function to trace the flow.
**   DBG(msg)            -> general trace: DBG("bytes=" << n << " fd=" << fd)
**   DBG_ERR(msg)        -> error trace: prints your message PLUS
**                          strerror(errno), i.e. WHY the system call failed
**                          (e.g. "bind() failed: Address already in use").
**   DBG_IN(fd, line)    -> raw IRC line RECEIVED from client <fd>  ("<<")
**   DBG_OUT(fd, line)   -> raw IRC line QUEUED for client <fd>     (">>")
**   DBG_EVENT(msg)      -> poll()/socket event (connect, disconnect, POLLIN..)
**
** Why macros and not functions?
**   - When DEBUG_MODE is off the preprocessor deletes the whole statement,
**     including its arguments: the release binary contains zero debug code.
**   - __FILE__, __LINE__ and __FUNCTION__ expand at the CALL SITE, so the
**     trace shows where it was printed, not where the helper lives.
**
** All output goes to std::cerr so it is unbuffered (appears immediately,
** even if the server crashes right after) and can be split from normal
** output:  ./ircserv 6667 pass 2> debug.log
** ===========================================================================
*/

#ifdef DEBUG_MODE

# include <iostream>
# include <cstring>
# include <cerrno>

/* ANSI colors: only used in debug mode, only on stderr. */
# define DBG_RESET   "\033[0m"
# define DBG_RED     "\033[31m"   /* errors                */
# define DBG_GREEN   "\033[32m"   /* incoming data  ("<<") */
# define DBG_YELLOW  "\033[33m"   /* events                */
# define DBG_BLUE    "\033[34m"   /* function entry        */
# define DBG_CYAN    "\033[36m"   /* outgoing data  (">>") */
# define DBG_GREY    "\033[90m"   /* file:line location    */

/* The do { } while (0) wrapper makes each macro behave like ONE statement,
** so it stays safe inside an if/else without braces. */

# define DBG(msg) \
	do { \
		std::cerr << DBG_GREY << "[DBG] " << __FILE__ << ":" << __LINE__ \
				  << DBG_RESET << " " << msg << std::endl; \
	} while (0)

# define DBG_FUNC() \
	do { \
		std::cerr << DBG_BLUE << "[DBG] --> " << __FUNCTION__ << "()" \
				  << DBG_RESET << std::endl; \
	} while (0)

/* errno is set by the failed system call (socket, bind, recv, ...);
** strerror(errno) translates it to a human sentence: this is the WHY. */
# define DBG_ERR(msg) \
	do { \
		std::cerr << DBG_RED << "[DBG][ERROR] " << msg \
				  << " -- why: " << std::strerror(errno) \
				  << DBG_GREY << "  (" << __FILE__ << ":" << __LINE__ << ")" \
				  << DBG_RESET << std::endl; \
	} while (0)

# define DBG_IN(fd, line) \
	do { \
		std::cerr << DBG_GREEN << "[DBG] << fd " << fd << " | " << line \
				  << DBG_RESET << std::endl; \
	} while (0)

# define DBG_OUT(fd, line) \
	do { \
		std::cerr << DBG_CYAN << "[DBG] >> fd " << fd << " | " << line \
				  << DBG_RESET << std::endl; \
	} while (0)

# define DBG_EVENT(msg) \
	do { \
		std::cerr << DBG_YELLOW << "[DBG][EVENT] " << msg \
				  << DBG_RESET << std::endl; \
	} while (0)

#else

/* Release build: the macros expand to an empty statement, arguments are
** never evaluated, the compiler removes all debug code. The do/while(0)
** shell (instead of pure nothing) keeps `if (x) DBG(...); else ...`
** legal and silences -Wempty-body. */
# define DBG(msg)			do { } while (0)
# define DBG_FUNC()			do { } while (0)
# define DBG_ERR(msg)		do { } while (0)
# define DBG_IN(fd, line)	do { } while (0)
# define DBG_OUT(fd, line)	do { } while (0)
# define DBG_EVENT(msg)		do { } while (0)

#endif

#endif
