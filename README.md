*This project has been created as part of the 42 curriculum by mmouqtad, mtarza, mohel-am.*

# ft_irc

## Description

ft_irc is an IRC server written in C++98. IRC clients connect to it over TCP. Users can join channels and send private messages. The goal of the project is to learn the TCP/IP protocols by building a real chat server.

The server runs in one thread and serves many clients at the same time. It does not fork and it never blocks. All sockets are non-blocking. One `poll()` call drives all the work: accept, read and write.

TCP gives a stream of bytes, not messages. The server keeps the bytes of each client in a separate buffer. It runs a command only when the full line has arrived. A command that is split over several packets is rebuilt first.

## Commands

The server implements every command the subject asks for, and nothing else. Any other command is answered with `421`.

Each table gives the raw form you type into `nc`, the irssi form, and the replies the server can send back.

### Registration

A client must send `PASS`, then `NICK` and `USER`, before any other command. The order of `NICK` and `USER` does not matter. Any other command before that is answered with `451`.

| Command | Raw form | In irssi | What it does | Replies |
| --- | --- | --- | --- | --- |
| `PASS` | `PASS <password>` | `irssi -c <host> -p <port> -w <password> -n <nick>` | Send the server password. irssi sends it while it connects, so you never type it. | `461` no parameter, `462` already registered |
| `NICK` | `NICK <nickname>` | `/nick <nickname>` | Set or change the nickname. A change is broadcast to every channel the client is in. | `431` no nickname given, `432` bad nickname, `433` nickname already in use |
| `USER` | `USER <username> 0 * :<real name>` | sent by irssi while it connects | Set the username. The server keeps the username only. Change what irssi sends with `/set user_name <name>` before you connect. | `461` fewer than four parameters, `462` already registered |

When the password is correct and both the nickname and the username are set, the server sends `001` and the client is registered. If the password is wrong, the server sends `464` and closes the connection after that reply has been sent.

Nicknames are compared without case, so `bob` and `BOB` are the same nickname. A nickname is refused if it is empty, if it contains a space, `!` or `@`, or if it starts with `:`, `#` or `&`.

Watch out for the colon when you test with `nc`. `NICK :bob` sets the nickname to `bob`, because the `:` opens the trailing parameter and is not part of the value. `NICK ::bob` is what actually asks for a nickname starting with a colon, and the server refuses it with `432`.

### Messaging

| Command | Raw form | In irssi | What it does | Replies |
| --- | --- | --- | --- | --- |
| `PRIVMSG` | `PRIVMSG <nickname> :<text>` | `/msg <nickname> <text>` | Send a private message to one user. | `401` no such nickname, `411` no recipient, `412` no text |
| `PRIVMSG` | `PRIVMSG <channel> :<text>` | type in the channel window, or `/msg <channel> <text>` | Send a message to a channel. The server forwards it to every other member and never echoes it back to the sender. | `403` no such channel, `404` you are not a member, `411` no recipient, `412` no text |
| `PING` | `PING <token>` | sent by irssi on its own | The server answers `PONG <token>`. | none |

The text after `:` is one parameter, so it keeps its spaces. A channel name starts with `#` or `&`.

### Channels

| Command | Raw form | In irssi | What it does | Replies |
| --- | --- | --- | --- | --- |
| `JOIN` | `JOIN <channel>` or `JOIN <channel> <key>` | `/join <channel>` or `/join <channel> <key>` | Join a channel. The server creates the channel if it does not exist, and the first client to join becomes its operator. | `403` bad channel name, `461` no parameter, `471` channel is full, `473` channel is invite-only, `475` wrong key |
| `TOPIC` | `TOPIC <channel>` to read, `TOPIC <channel> :<text>` to set | `/topic <text>` to set, `/quote TOPIC <channel>` to read | Show or change the channel topic. | `331` no topic set, `332` the topic, `403` no such channel, `442` you are not on that channel, `461` no parameter, `482` not operator while `+t` is set |

After a successful `JOIN` the server sends the channel topic (`331` or `332`), then the member list (`353`) and the end of that list (`366`). The server deletes a channel when its last member leaves.

### Channel operator commands

`KICK`, `INVITE` and `MODE` require operator status and answer `482` otherwise. `TOPIC` requires it only while the channel is `+t`.

| Command | Raw form | In irssi | What it does | Replies |
| --- | --- | --- | --- | --- |
| `KICK` | `KICK <channel> <nickname> :<reason>` | `/kick <channel> <nickname> [reason]` | Remove a client from the channel. The reason is optional. | `403` no such channel, `441` target is not on the channel, `442` you are not on that channel, `461` missing parameters, `482` not operator |
| `INVITE` | `INVITE <nickname> <channel>` | `/invite <nickname> <channel>` | Invite a client to the channel. The server answers `341` and sends an `INVITE` to the target. | `341` invite sent, `401` no such nickname, `403` no such channel, `442` you are not on that channel, `443` target is already on the channel, `461` missing parameters, `482` not operator |
| `MODE` | `MODE <channel> <modes> [arguments]` | `/mode <channel> <modes>` | Change the channel modes, see the table below. | `403` no such channel, `441` target is not on the channel, `442` you are not on that channel, `461` a mode needs an argument, `472` unknown mode letter, `482` not operator |

Several mode letters can be combined in one command, and `+` and `-` can be mixed: `MODE #room +it`, `MODE #room -ko bob`. Each letter that needs an argument takes the next one in order. `MODE <channel>` with no mode letters does nothing.

### Channel modes

| Mode | Argument | Raw form | In irssi | What it does |
| --- | --- | --- | --- | --- |
| `i` | none | `MODE <channel> +i` / `-i` | `/mode <channel> +i` / `-i` | Invite-only. A client that was not invited is refused with `473`. |
| `t` | none | `MODE <channel> +t` / `-t` | `/mode <channel> +t` / `-t` | Only operators may change the topic. Everyone else gets `482`. |
| `k` | on `+k` only | `MODE <channel> +k <key>` / `-k` | `/mode <channel> +k <key>` / `-k` | Set or remove the channel key. A client that does not give the key is refused with `475`. |
| `o` | always | `MODE <channel> +o <nickname>` / `-o <nickname>` | `/op <nickname>` / `/deop <nickname>` | Give or take operator status. The target must be on the channel, otherwise `441`. |
| `l` | on `+l` only | `MODE <channel> +l <number>` / `-l` | `/mode <channel> +l <number>` / `-l` | Limit the number of members. A client that arrives when the channel is full is refused with `471`. `-l` removes the limit. |

### Two commands the server answers but does not implement

irssi sends `CAP LS` before it sends `PASS`, and `WHO` by itself after every join. The user types neither. The server accepts both and replies nothing, because the subject requires the reference client to connect without encountering any error. They add no feature.

### Numeric replies

| Code | Meaning | Code | Meaning |
| --- | --- | --- | --- |
| `001` | welcome, you are registered | `433` | nickname already in use |
| `331` | no topic is set | `441` | that user is not on that channel |
| `332` | the channel topic | `442` | you are not on that channel |
| `341` | your invite was sent | `443` | that user is already on that channel |
| `353` | member list | `451` | you have not registered |
| `366` | end of the member list | `461` | not enough parameters |
| `401` | no such nickname | `462` | you may not reregister |
| `403` | no such channel | `464` | password incorrect |
| `404` | cannot send to that channel | `471` | channel is full |
| `411` | no recipient given | `472` | unknown mode letter |
| `412` | no text to send | `473` | channel is invite-only |
| `421` | unknown command | `475` | wrong channel key |
| `431` | no nickname given | `482` | you are not a channel operator |
| `432` | bad nickname | | |

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
