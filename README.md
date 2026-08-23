*This project has been created as part of the 42 curriculum by mmouqtad, mtarza.*

# ft_irc

## Description

ft_irc is an IRC server written in C++98. The goal is to understand the TCP/IP
protocols that the Internet runs on by implementing a real text chat server
that an actual IRC client can connect to and use.

The server handles many clients at the same time from a single thread. There is
no forking and no blocking I/O: every socket is non-blocking and one `poll()`
call drives everything — accepting connections, reading, and writing. Because
TCP is a byte stream and not a sequence of messages, incoming bytes are
accumulated per client and only complete lines are executed, so a command split
across several packets is rebuilt before it runs.

## Features

Registration and messaging:

| Command | Description |
| --- | --- |
| `PASS` | Give the server password. |
| `NICK` | Set or change the nickname. |
| `USER` | Set the username and real name. |
| `PRIVMSG` | Message a user, or a channel (forwarded to every other member). |
| `JOIN` | Join a channel, creating it if needed. The creator becomes operator. |
| `PING` | Answered with `PONG`. |

Channel operator commands:

| Command | Description |
| --- | --- |
| `KICK` | Eject a client from the channel. |
| `INVITE` | Invite a client to a channel. |
| `TOPIC` | Change or view the channel topic. |
| `MODE` | Change the channel mode. |

Channel modes, all implemented:

| Mode | Description |
| --- | --- |
| `i` | Invite-only: joining requires an invitation. |
| `t` | Only operators may change the topic. |
| `k` | Channel key, required to join. |
| `o` | Give or take channel operator privilege. |
| `l` | User limit on the channel. |

## Instructions

Build (no external library is needed):

```sh
make
```

Other rules: `make clean`, `make fclean`, `make re`.

Run:

```sh
./ircserv <port> <password>
```

- `port` — the TCP port to listen on, 1024 to 65535.
- `password` — the password every client must send with `PASS`.

Connect with the reference client, irssi:

```sh
irssi -c 127.0.0.1 -p 6667 -w <password> -n <nickname>
/join #test
/msg <nickname> hello
```

Or by hand with netcat:

```sh
nc -C 127.0.0.1 6667
PASS <password>
NICK alice
USER alice 0 * :Alice
JOIN #test
PRIVMSG #test :hello everyone
```

## Technical choices

- **One `poll()` for everything.** The listening socket and every client socket
  live in the same pollfd vector. `recv()` runs only when `POLLIN` is reported,
  `send()` only when `POLLOUT` is reported, and `accept()` only when the
  listening socket is readable. Nothing ever touches a file descriptor that
  `poll()` has not declared ready.
- **Two buffers per client.** Received bytes go into an input buffer and are
  cut into lines on `\n`, leaving any partial line for the next read; both
  `\r\n` and a bare `\n` are accepted. Replies are never sent immediately, they
  are appended to an output buffer that is flushed when the socket becomes
  writable, which also handles a partial `send()`.
- **Registration is a gate.** A client must send `PASS`, `NICK` and `USER`
  before anything else works. Only then is `001` sent. A wrong password is
  answered with `464` and the link is closed once that reply has been flushed.
- **Channels hold file descriptors,** not pointers to clients, so a
  disconnecting client cannot leave a dangling reference behind. A channel is
  destroyed as soon as its last member leaves.

## Resources

- [RFC 1459 — Internet Relay Chat Protocol](https://datatracker.ietf.org/doc/html/rfc1459)
- [RFC 2812 — Internet Relay Chat: Client Protocol](https://datatracker.ietf.org/doc/html/rfc2812)
- [Modern IRC Client Protocol](https://modern.ircdocs.horse/)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- `man 2 poll`, `man 2 socket`, `man 2 recv`, `man 2 send`, `man 2 fcntl`

**How AI was used.** AI (Claude) was used to summarise the IRC RFCs and the
subject, to help design and write the line parser, the per-client buffering,
the `poll()` read/write loop and the command handlers, and to generate the test
programs used during development: unit tests that feed fake strings to the
parser and the buffer, an end-to-end test that drives the server over real
sockets through every required command and error case, and a fuzz test that
throws malformed and random binary input at it. AI was also used to check the
behaviour against the reference client and to run the code under valgrind. Every
generated line was reviewed, tested and is understood by the authors.
