*This project has been created as part of the 42 curriculum by mmouqtad, mtarza, mohel-am.*

# ft_irc

## Description

ft_irc is an IRC server written in C++98. IRC clients connect to it over TCP. Users can join channels and send private messages. The goal of the project is to learn the TCP/IP protocols by building a real chat server.

The server runs in one thread and serves many clients at the same time. It does not fork and it never blocks. All sockets are non-blocking. One `poll()` call drives all the work: accept, read and write.

TCP gives a stream of bytes, not messages. The server keeps the bytes of each client in a separate buffer. It runs a command only when the full line has arrived. A command that is split over several packets is rebuilt first.

## Features

Every command below is implemented by the server. The irssi column shows how to reach it from the reference client.

Registration and messaging:

| Command | In irssi | Description |
| --- | --- | --- |
| `PASS` | `irssi -c 127.0.0.1 -p 6667 -w <password> -n <nick>` | Send the server password. irssi sends it while it connects, so you never type it. |
| `NICK` | `/nick <nickname>` | Set or change the nickname. |
| `USER` | sent by irssi while it connects | Set the username. Change what irssi sends with `/set user_name <name>` before you connect. |
| `JOIN` | `/join #channel` or `/join #channel <key>` | Join a channel. The server creates the channel if it does not exist, and the first client to join becomes an operator. |
| `PRIVMSG` | type in the channel window, or `/msg #channel <text>` | Send a message to a channel. The server forwards it to every other member. |
| `PRIVMSG` | `/msg <nickname> <text>` | Send a private message to one user. |
| `PING` | sent by irssi on its own | The server answers with `PONG`. |

Channel operator commands. A user who is not an operator gets `482` from `KICK`, `INVITE` and `MODE`. `TOPIC` only needs operator status while the channel is `+t`:

| Command | In irssi | Description |
| --- | --- | --- |
| `KICK` | `/kick #channel <nickname> [reason]` | Remove a client from the channel. |
| `INVITE` | `/invite <nickname> #channel` | Invite a client to the channel. |
| `TOPIC` | `/topic <text>` to change it, `/quote TOPIC #channel` to read it | Show or change the channel topic. |
| `MODE` | `/mode #channel <modes>` | Change the channel mode, see the table below. |

Channel modes, all five implemented:

| Mode | In irssi | Description |
| --- | --- | --- |
| `i` | `/mode #channel +i` and `/mode #channel -i` | Invite-only. A client must be invited to join, and is refused with `473`. |
| `t` | `/mode #channel +t` and `/mode #channel -t` | Only operators can change the topic. Everyone else gets `482`. |
| `k` | `/mode #channel +k <key>` and `/mode #channel -k` | The channel has a key. A client that does not give it is refused with `475`. `-k` takes no key. |
| `o` | `/op <nickname>` and `/deop <nickname>` | Give or take operator status. Both directions need the nickname. |
| `l` | `/mode #channel +l <number>` and `/mode #channel -l` | Limit the number of members. A client that arrives when the channel is full is refused with `471`. `-l` removes the limit. |

Three notes on irssi itself. `/kick`, `/invite` and `/mode` use the channel of the active window when you leave the channel out, so name the channel to be safe. `/topic` with no text prints what irssi remembers and sends nothing, which is why the table gives `/quote TOPIC #channel` instead. `/help` and `/names` are answered by irssi alone and never reach the server.

Any other command is answered with `421`.

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

- `port`: the TCP port to listen on. Use a value from 1 to 65535. A port below 1024 needs root rights.
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
