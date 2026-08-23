*This project has been created as part of the 42 curriculum by mmouqtad, mtarza.*

# ft_irc

## Description

ft_irc is an IRC server written in C++98. IRC clients connect to it over TCP. Users can join channels and send private messages. The goal of the project is to learn the TCP/IP protocols by building a real chat server.

The server runs in one thread and serves many clients at the same time. It does not fork and it never blocks. All sockets are non-blocking. One `poll()` call drives all the work: accept, read and write.

TCP gives a stream of bytes, not messages. The server keeps the bytes of each client in a separate buffer. It runs a command only when the full line has arrived. A command that is split over several packets is rebuilt first.

## Features

Registration and messaging:

| Command | Description |
| --- | --- |
| `PASS` | Send the server password. |
| `NICK` | Set or change the nickname. |
| `USER` | Set the username. |
| `PRIVMSG` | Send a message to a user or to a channel. The server forwards a channel message to all other members. |
| `JOIN` | Join a channel. The server creates the channel if it does not exist. The first client to join becomes an operator. |
| `PING` | The server answers with `PONG`. |

Channel operator commands:

| Command | Description |
| --- | --- |
| `KICK` | Remove a client from the channel. |
| `INVITE` | Invite a client to the channel. |
| `TOPIC` | Show or change the channel topic. |
| `MODE` | Change the channel mode. |

Channel modes, all implemented:

| Mode | Description |
| --- | --- |
| `i` | Invite-only. A client must be invited to join. |
| `t` | Only operators can change the topic. |
| `k` | The channel has a key. A client must give the key to join. |
| `o` | Give or remove operator status. |
| `l` | Limit the number of members. |

## Instructions

Build the server. No external library is needed.

```sh
make
```

The Makefile also has the rules `clean`, `fclean` and `re`.

Run the server:

```sh
./ircserv <port> <password>
```

- `port`: the TCP port to listen on. Use a value from 1024 to 65535.
- `password`: the password that each client must send with `PASS`.

Connect with irssi, the reference client:

```sh
irssi -c 127.0.0.1 -p 6667 -w <password> -n <nickname>
/join #test
/msg <nickname> hello
```

You can also connect by hand with netcat:

```sh
nc -C 127.0.0.1 6667
PASS <password>
NICK alice
USER alice 0 * :Alice
JOIN #test
PRIVMSG #test :hello everyone
```

## Technical choices

- **One `poll()` for everything.** The listening socket and all client sockets are in the same pollfd vector. The server calls `recv()` only after `poll()` reports `POLLIN`. It calls `send()` only after `poll()` reports `POLLOUT`. It calls `accept()` only when the listening socket is ready.
- **Two buffers for each client.** The server adds the bytes it reads to an input buffer. It then cuts the buffer into lines at each `\n` and keeps the last partial line for the next read. It accepts `\r\n` and a single `\n`. The server never sends a reply at once. It adds the reply to an output buffer and sends that buffer when the socket is ready to write. This also handles a partial `send()`.
- **Registration is a gate.** A client must send `PASS`, `NICK` and `USER` before it can use any other command. The server then sends `001`. If the password is wrong, the server sends `464` and closes the connection after that reply is sent.
- **Channels store file descriptors.** A channel stores file descriptors, not pointers to clients. A client that disconnects cannot leave an invalid pointer behind. The server deletes a channel when the last member leaves.

## Resources

- [RFC 1459 — Internet Relay Chat Protocol](https://datatracker.ietf.org/doc/html/rfc1459)
- [RFC 2812 — Internet Relay Chat: Client Protocol](https://datatracker.ietf.org/doc/html/rfc2812)
- [Modern IRC Client Protocol](https://modern.ircdocs.horse/)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- `man 2 poll`, `man 2 socket`, `man 2 recv`, `man 2 send`, `man 2 fcntl`

**How AI was used.** AI was used to summarise the IRC RFCs and the subject, to help design and plan the project. AI was also used to check the behaviour against the reference client and to run the code under valgrind.
