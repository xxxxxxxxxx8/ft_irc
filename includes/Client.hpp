#ifndef CLIENT_HPP
# define CLIENT_HPP

# include <string>

// Maximum length of a single IRC line (RFC: 512 bytes including \r\n).
// If a client's buffer grows past this with no newline, it's misbehaving.
# define MAX_LINE_LEN 512

// One connected client. Owns its own receive buffer so that partial TCP
// packets from THIS client never mix with anyone else's (section 5 of the plan).
class Client
{
	private:
		int			_fd;			// socket file descriptor
		std::string	_ip;			// client IP (for the nick!user@host prefix)
		std::string	_buffer;		// leftover bytes between recv() calls

		// registration state
		std::string	_nick;
		std::string	_username;
		std::string	_realname;
		bool		_hasPass;		// sent a correct PASS?
		bool		_registered;	// PASS + NICK + USER all done -> 001 sent

	public:
		Client(int fd, const std::string &ip);
		~Client();

		// --- buffer logic (the important part) ---
		// Append freshly recv()'d bytes to this client's buffer.
		void	appendData(const char *data, size_t len);
		// Extract one complete line (without trailing \r\n) into `line`.
		// Returns false and leaves the buffer untouched if no full line yet.
		bool	getNextLine(std::string &line);
		// True if the buffer overflowed MAX_LINE_LEN with no newline.
		bool	bufferOverflow() const;

		// --- getters ---
		int					getFd() const;
		const std::string	&getIp() const;
		const std::string	&getNick() const;
		const std::string	&getUsername() const;
		const std::string	&getRealname() const;
		bool				hasPass() const;
		bool				isRegistered() const;

		// --- setters ---
		void	setNick(const std::string &nick);
		void	setUsername(const std::string &username);
		void	setRealname(const std::string &realname);
		void	setHasPass(bool v);
		void	setRegistered(bool v);
};

#endif
