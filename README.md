*This project has been created as part of the 42 curriculum by mtarza.*

# ft_irc

## Description

ft_irc is an IRC (Internet Relay Chat) server written in C++98. Its goal is to
understand the TCP/IP network protocols that govern the Internet by
implementing a real, standard-compliant text chat server that actual IRC
clients can connect to.

The server handles multiple clients simultaneously using a single `poll()`
loop and non-blocking sockets only — no forking, no blocking I/O. Incoming
packets are aggregated per client so commands sent in several fragments are
correctly rebuilt.

Implemented features:

- Connection registration with a password (`PASS`, `NICK`, `USER`)
- Channels: `JOIN`, `PART`, message forwarding to every channel member
- Private messages between users (`PRIVMSG`, `NOTICE`)
- Channel operators and the operator commands:
  - `KICK` — eject a client from the channel
  - `INVITE` — invite a client to a channel
  - `TOPIC` — change or view the channel topic
  - `MODE` — channel modes `i` (invite-only), `t` (topic restricted to
    operators), `k` (channel key), `o` (operator privilege), `l` (user limit)
- `PING`/`PONG` and `QUIT`

Bonus:

- **File transfer**: DCC file transfers work between clients; the server
  relays the DCC handshake (a CTCP `PRIVMSG`) untouched, exactly like an
  official IRC server, and the transfer then happens directly between the
  two clients.
- **A bot**: a built-in bot named `marvin` is always available. Talk to it
  with `/msg marvin !help` (`!help`, `!time`, `!ping`).

## Workflow — how the server works

Everything is driven by one `poll()` event loop. From start to shutdown:

```
main()                        parse & validate <port> and <password>
  |
  v
Server::setup()               socket() -> setsockopt(SO_REUSEADDR)
  |                           -> O_NONBLOCK -> bind() -> listen()
  |                           + install SIGINT/SIGQUIT handlers
  v
Server::run()  <============= the event loop (runs until Ctrl-C)
  |
  |  poll() sleeps until one fd is "ready", then for each ready fd:
  |
  |-- listen fd, POLLIN  ---> acceptClient()      new connection, new Client
  |-- client fd, POLLIN  ---> receiveData()       recv() -> recvBuffer
  |       |                                       split on '\n' into lines
  |       v
  |   handleLine()  --------> splitLine() parses "CMD params :trailing"
  |       |                   then dispatches to cmdPass/cmdNick/cmdUser/
  |       |                   cmdJoin/cmdPrivmsg/cmdMode/...
  |       v
  |   reply()/broadcast() --> queueSend()         text -> sendBuffer
  |                                               + enable POLLOUT on the fd
  |-- client fd, POLLOUT ---> sendData()          send() as much as possible,
  |                                               disable POLLOUT when empty
  |-- POLLERR/POLLHUP    ---> disconnectClient()  notify peers, free Client
  v
SIGINT/SIGQUIT sets _signal -> loop exits -> destructor closes everything
```

Key ideas:

- **Nothing ever blocks.** All sockets are `O_NONBLOCK`; the only place the
  server waits is inside `poll()`.
- **One buffer in, one buffer out per client.** `recv()` may deliver half a
  command or three commands at once: bytes are accumulated in `recvBuffer`
  and only complete `\n`-terminated lines are executed. Replies are never
  `send()` directly: they are queued in `sendBuffer` and flushed when
  `poll()` reports the socket writable (`POLLOUT`).
- **Registration is a small state machine.** A client becomes registered
  only once `PASS` (correct), `NICK` and `USER` were all received; a wrong
  password answers `464` and closes the link.

### Debug mode

The whole flow above can be watched live. Build with:

```sh
make debug
./ircserv 6667 pass          # traces on stderr, normal output on stdout
./ircserv 6667 pass 2> debug.log   # or keep the traces in a file
```

Debug traces are printed by the `DBG*` macros of `includes/Debug.hpp`:
function entry (`DBG_FUNC`), every raw IRC line in (`<<`) and out (`>>`),
poll/connection events, and each failed system call **with the reason**
(`strerror(errno)`, e.g. `bind() failed -- why: Address already in use`).
A normal `make` compiles all of this to nothing — zero cost in the
evaluation binary.

## Debugging toolbox

Everything below assumes the server is running: `./ircserv 6667 pass`

### Find the server process

```sh
pidof ircserv                # just the PID
ps aux | grep ircserv        # PID + CPU/memory usage + full command line
```

### Show the sockets (ss / netstat / lsof)

`ss` is the modern tool (`netstat` is its older equivalent, same job):

```sh
ss -tlnp | grep 6667         # the LISTENING socket
#  LISTEN 0  4096  0.0.0.0:6667  0.0.0.0:*  users:(("ircserv",pid=13062,fd=3))
#  ^state    ^backlog            ^accepts from anywhere   ^which process + which fd

ss -tnp | grep 6667          # every ESTABLISHED client connection
#  ESTAB 0 0  127.0.0.1:6667  127.0.0.1:48690  users:(("ircserv",fd=4))
#  one line per connected client; fd=4 matches the fd in the DBG traces

# flags: -t TCP only, -l listening only, -n numeric ports, -p show process
netstat -tlnp | grep 6667    # exact same info, older syntax
```

`lsof` lists open files — and a socket IS a file descriptor:

```sh
lsof -p $(pidof ircserv)     # ALL fds of the server: 0-2 tty, 3 listen socket,
                             # 4+ one per connected client
lsof -i :6667                # who is using port 6667 (server AND clients)
ls -l /proc/$(pidof ircserv)/fd   # same view straight from the kernel
```

This is the fastest way to check for **fd leaks**: connect and disconnect
clients, then verify the fd count went back down.

### Watch the system calls live (strace)

See exactly what the process asks the kernel — poll waking up, recv, send:

```sh
strace -p $(pidof ircserv)                       # attach to running server
strace -e trace=network ./ircserv 6667 pass      # only network syscalls
strace -f -e poll,accept4,recvfrom,sendto ./ircserv 6667 pass
```

You will literally see the workflow diagram above happening:
`poll(...) = 1` → `accept4(3, ...) = 4` → `recvfrom(4, "NICK momo\r\n"...)`.

### Watch the raw IRC traffic on the wire (tcpdump)

```sh
sudo tcpdump -i lo -A 'port 6667'    # -A prints packet payload as ASCII
```

Every `PRIVMSG`, every numeric reply, exactly as it crosses the network.

### Debug a crash (gdb)

`make debug` compiles with `-g3`, so gdb shows source lines:

```sh
make debug
gdb ./ircserv
(gdb) run 6667 pass                  # start under gdb; on crash: where am I?
(gdb) break Server::handleLine      # stop every time a command arrives
(gdb) break srcs/Server.cpp:104     # or stop at a file:line
(gdb) continue                       # resume until next breakpoint
(gdb) print client->getNickname()   # inspect any variable
(gdb) backtrace                      # the call chain that led here
gdb -p $(pidof ircserv)             # or attach to an already-running server
```

### Check memory leaks (valgrind)

```sh
valgrind --leak-check=full --show-leak-kinds=all ./ircserv 6667 pass
# connect some clients, JOIN/PART/QUIT, then Ctrl-C the server:
# the report must say "no leaks are possible" / 0 bytes definitely lost
valgrind --track-fds=yes ./ircserv 6667 pass   # also reports leaked fds
```

### Talk to the server by hand (nc)

```sh
nc -C 127.0.0.1 6667          # -C sends \r\n line endings like a real client
PASS pass
NICK momo
USER momo 0 * :Momo
JOIN #test
PRIVMSG #test :hello
```

Test the **partial-command aggregation** (a required subject case): type
`PAS`, press Ctrl-D (sends without newline), type `S pass`, press Enter —
the server must rebuild `PASS pass`. With `make debug` you see it happen:
two `recv` traces, then one `<< fd 4 | PASS pass`.

### Stop / signal the server

```sh
kill -INT  $(pidof ircserv)   # same as Ctrl-C  -> clean shutdown
kill -QUIT $(pidof ircserv)   # same as Ctrl-\  -> clean shutdown too
```

### Common errors and what they mean

| Error | Why | Fix |
|---|---|---|
| `bind() failed` — Address already in use | Port taken by another process (often a previous ircserv) | `lsof -i :6667` to find it, kill it, or use another port |
| `Connection refused` from nc/irssi | No server listening on that port | check `ss -tlnp`, start the server |
| Client hangs at connect with irssi | irssi sends `CAP LS` first; server must ignore it (we do) | check the DBG dispatch trace |
| fd count keeps growing | disconnected clients not cleaned up | `lsof -p PID` after QUIT, compare counts |
| `poll() failed` | real poll error (EINTR is already handled) | run under strace to see the errno |

## Instructions

Compilation (requires a C++98 compiler, no external library):

```sh
make
```

Execution:

```sh
./ircserv <port> <password>
```

- `port`: the listening port (1024–65535)
- `password`: the password clients must provide with `PASS` to register

Then connect with an IRC client (irssi is the reference client):

```sh
irssi
/connect 127.0.0.1 <port> <password> <nickname>
/join #mychannel
```

You can also test raw commands with netcat:

```sh
nc -C 127.0.0.1 <port>
PASS <password>
NICK mynick
USER myuser 0 * :My Name
JOIN #test
```

## Resources

- [RFC 1459 — Internet Relay Chat Protocol](https://datatracker.ietf.org/doc/html/rfc1459)
- [RFC 2812 — Internet Relay Chat: Client Protocol](https://datatracker.ietf.org/doc/html/rfc2812)
- [Modern IRC Client Protocol documentation](https://modern.ircdocs.horse/)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- `man 2 socket`, `man 2 poll`, `man 7 ip`

**How AI was used**: AI (Claude) was used to read and summarize the IRC RFCs,
to help design the command parser and the poll()-based event loop, to write
repetitive boilerplate (getters/setters, numeric reply strings), and to
generate a functional test script exercising registration, channel modes and
operator commands. All generated code was reviewed, tested manually with
irssi and netcat, and is fully understood by the author.
