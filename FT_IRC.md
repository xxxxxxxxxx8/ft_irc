# ft_irc — Project Guide & Team Plan

A single reference for the whole project: what you're building, how IRC works, the hard parts (especially the per-client buffer), the commands to implement, and how to split the work between 3 people.

---

## 1. What the project is

You build an **IRC server** in **C++98**. That's it. Two hard rules from the subject:

- You do **NOT** build a client. You test with a real one (irssi, weechat, HexChat…).
- You do **NOT** implement server-to-server. Only client ↔ your server.

Run it like this:

```
./ircserv <port> <password>
```

- `port` — the TCP port your server listens on.
- `password` — every client must send this to connect.

IRC is a text chat protocol from 1988 — the ancestor of Slack/Discord. The model is dead simple: **many clients connect to one server over TCP, and the server relays text between them.** Clients never talk to each other directly. A user sends a line, the server decides who should get it, and forwards it.

Two ways people talk:
- **Private message** — one user directly to another user.
- **Channel** — a group room (name starts with `#`, e.g. `#general`). Anything sent to a channel is forwarded to everyone in it.

---

## 2. How IRC works (the protocol)

### 2.1 The message format

IRC is **line-based**. Every message is one line ending in `\r\n`. Format:

```
[:prefix] COMMAND [params...] [:trailing]
```

- **COMMAND** — a word like `NICK`, `JOIN`, `PRIVMSG`.
- **params** — space-separated arguments.
- **trailing** — the last param, prefixed with `:`. It's the **only** param allowed to contain spaces (used for messages, topics…).
- **prefix** — starts with `:`, says who a message came from. **Clients don't send it — your server adds it** when relaying.

Example — a client sends:
```
PRIVMSG #general :Hello everyone!
```
Your server relays it to the other members as:
```
:alice!alice@localhost PRIVMSG #general :Hello everyone!
```
That `nick!user@host` prefix is how receivers know who spoke. Getting this format exactly right is what makes real clients accept your server.

### 2.2 The registration handshake

When a client connects it can't do anything until it **registers**, in this order:

1. `PASS <password>` — the server password (2nd CLI arg). Wrong → reject with `464`.
2. `NICK <nickname>` — pick a nick. Taken → reject with `433`.
3. `USER <username> 0 * :<realname>` — set username + real name.

Once all three are done, send the welcome reply `001` and the client is "in." Until then, any other command is rejected with `451` (not registered).

### 2.3 Channels & operators

A channel is created the moment the first person `JOIN`s it — and **that first person becomes the operator** (the room admin). Operators have powers regular users don't. Your server tracks per channel: members, who's an operator, the topic, the modes, an optional key, and a user limit.

---

## 3. The data flow (one message, start to finish)

```
IRC clients            send lines over TCP
      |
      v
poll() loop            one loop watches every socket (accept + recv)   [Person A]
      |
      v
per-client buffer      reassemble split TCP packets into full lines     [Person B]
      |
      v
line parser            split a line into COMMAND + args                 [Person B]
      |
      v
command dispatch       route to the right handler                       [Person C]
      |
      v
broadcast              forward to the channel's members                 [Person C]
      |
      +---> relayed back out to the other clients
```

Everything you implement is either making one of these boxes work, or making the server **talk back** correctly with numeric replies at each step.

---

## 4. The engine (sockets + poll)

This is the hardest layer. Three constraints drive the whole design:

- **Multiple clients at once, never hang.** You can't `recv()` from one client and block — everyone else freezes.
- **All I/O is non-blocking**, and **forking is banned**.
- **Exactly one `poll()`** handles *everything*: the listening socket **and** every client socket, for both reading and writing.

`poll()` is the trick: give it a list of all your file descriptors, and it tells you which are *ready right now* (new connection waiting? data to read? ready to write?). You act only on the ready ones, then loop. One thread, no blocking, handles hundreds of clients.

Startup sequence for the listening socket:
```
socket()  ->  setsockopt(SO_REUSEADDR)  ->  bind()  ->  listen()  ->  fcntl(fd, F_SETFL, O_NONBLOCK)
```
`SO_REUSEADDR` avoids the "address already in use" error when you restart during testing.

> **The rule that fails people:** never `recv`/`send` on any fd without `poll()` telling you it's ready. Do it once and your grade is 0.

---

## 5. The per-client buffer for partial packets (the important part)

### 5.1 Why you even need this

**TCP is a stream, not messages.** When you `recv()`, the kernel gives you *whatever bytes have arrived* — which has nothing to do with where commands begin or end. One `recv()` can give you:

- **Half a command:** `recv` returns `"NIC"` — the rest hasn't arrived yet.
- **Multiple commands at once:** `recv` returns `"NICK bob\r\nUSER b 0 * :Bob\r\n"`.
- **A command split across several `recv`s:** `"com"`, then `"man"`, then `"d\r\n"`.

That last case is exactly the subject's `nc` test — you send `com`, `man`, `d\n` in three pieces with Ctrl+D. If you try to parse the raw bytes from a single `recv`, you break on all three cases.

### 5.2 The fix: accumulate, then extract complete lines

Each client owns a `std::string` buffer. On every `recv`:

1. **Append** the received bytes to that client's buffer.
2. **Extract** every *complete* line (ending in a newline) and process each.
3. **Leave** any leftover partial line in the buffer for next time.

The golden rule: **never process on `recv` boundaries — only on newline boundaries.**

### 5.3 It MUST be per-client (not one global buffer)

This is the whole point of "per-client." Clients interleave. Imagine one shared buffer:

- Client A sends `"NIC"`
- Client B sends `"USER b 0 * :Bob\r\n"`
- Client A sends `"K alice\r\n"`

A global buffer would hold `"NICUSER b 0 * :BobK alice"` — total garbage. Each client's partial data must be **isolated in that client's own object**, keyed by its fd.

### 5.4 Code (C++98)

Store the buffer in the Client class:

```cpp
class Client {
    // ...
    std::string _buffer;   // holds leftover bytes between recv() calls
public:
    void appendData(const char *data, size_t len);
    bool getNextLine(std::string &line);
};
```

Append incoming bytes:

```cpp
void Client::appendData(const char *data, size_t len) {
    _buffer.append(data, len);
}
```

Pull out one complete line at a time. This handles **both** `\r\n` (real clients) and bare `\n` (the `nc` test), by finding `\n` then stripping a trailing `\r` if present:

```cpp
bool Client::getNextLine(std::string &line) {
    std::string::size_type pos = _buffer.find('\n');
    if (pos == std::string::npos)
        return false;                    // no complete line yet -> wait

    line = _buffer.substr(0, pos);       // take everything before '\n'
    _buffer.erase(0, pos + 1);           // remove it AND the '\n' from the buffer

    if (!line.empty() && line[line.size() - 1] == '\r')
        line.erase(line.size() - 1);     // drop trailing '\r' (handles \r\n)

    return true;
}
```

Use it in the poll loop when a client fd is readable:

```cpp
char buf[512];
ssize_t n = recv(fd, buf, sizeof(buf), 0);

if (n == 0) {
    // client disconnected -> remove fd, close, delete Client
} else if (n < 0) {
    // error handling
} else {
    client.appendData(buf, n);

    std::string line;
    while (client.getNextLine(line)) {   // drains ALL complete lines
        handleCommand(client, line);     // parse + dispatch one command
    }
    // any partial leftover stays in _buffer automatically
}
```

Walk it through the three cases:

- **`"NIC"` arrives** → `find('\n')` fails → `getNextLine` returns false → buffer keeps `"NIC"`, nothing processed. ✅
- **`"NICK bob\r\nUSER b 0 * :Bob\r\n"` arrives** → the `while` loop runs twice, extracting `NICK bob` then `USER b 0 * :Bob`. ✅
- **`"com"` then `"man"` then `"d\r\n"`** → buffer grows `"com"` → `"comman"` → `"command\r\n"`; only on the third `recv` does a line come out: `"command"`. ✅

### 5.5 One safety note

A malicious client could send megabytes with no newline to make your buffer grow forever. IRC's max line is **512 bytes**. Cap it: if a client's buffer exceeds a sane limit (e.g. 512) with still no newline, error the client out instead of letting it grow. Not strictly required to pass, but it's the kind of robustness that impresses in defense.

---

## 6. Commands you must implement

**Registration / basic**
| Command | What it does |
|---|---|
| `PASS` | Verify the connection password. |
| `NICK` | Set / change nickname (reject duplicates). |
| `USER` | Set username + real name. |
| `PRIVMSG` | Message to a user **or** to a channel. |

**Channel**
| Command | What it does |
|---|---|
| `JOIN` | Join (or create) a channel. First joiner = operator. |

**Operator-only** (must enforce the permission check):
| Command | What it does |
|---|---|
| `KICK` | Force a user out of the channel. |
| `INVITE` | Invite a user (needed for invite-only channels). |
| `TOPIC` | View or change the channel topic. |
| `MODE` | Change channel modes (see below). |

### Channel modes (the `MODE` command)
| Mode | Meaning |
|---|---|
| `i` | Invite-only — can't `JOIN` without an invite. |
| `t` | Only operators can change the topic. |
| `k` | Channel key — a password required to join. |
| `o` | Give / take operator status on a user. |
| `l` | Set / remove a user limit on the channel. |

---

## 7. Numeric replies you'll need

The server answers clients with 3-digit codes. Format:
```
:ircserv <code> <nick> <params> :<message>
```
Example: `:ircserv 433 * bob :Nickname is already in use`

Put ALL of these in **one shared header** everyone imports:

| Code | Name | When |
|---|---|---|
| 001 | RPL_WELCOME | Registration complete |
| 331 | RPL_NOTOPIC | Channel has no topic |
| 332 | RPL_TOPIC | Show the topic |
| 353 | RPL_NAMREPLY | Names list on JOIN |
| 366 | RPL_ENDOFNAMES | End of names list |
| 401 | ERR_NOSUCHNICK | Target nick doesn't exist |
| 403 | ERR_NOSUCHCHANNEL | Channel doesn't exist |
| 431 | ERR_NONICKNAMEGIVEN | NICK with no nick |
| 432 | ERR_ERRONEUSNICKNAME | Invalid nick |
| 433 | ERR_NICKNAMEINUSE | Nick already taken |
| 441 | ERR_USERNOTINCHANNEL | Target isn't in that channel |
| 442 | ERR_NOTONCHANNEL | You're not in that channel |
| 443 | ERR_USERONCHANNEL | INVITE target already in channel |
| 451 | ERR_NOTREGISTERED | Command before registration |
| 461 | ERR_NEEDMOREPARAMS | Not enough params |
| 462 | ERR_ALREADYREGISTERED | Re-sending PASS/USER after registered |
| 464 | ERR_PASSWDMISMATCH | Wrong server password |
| 471 | ERR_CHANNELISFULL | Channel at user limit (`+l`) |
| 472 | ERR_UNKNOWNMODE | Unknown mode char |
| 473 | ERR_INVITEONLYCHAN | Can't join invite-only (`+i`) |
| 475 | ERR_BADCHANNELKEY | Wrong channel key (`+k`) |
| 482 | ERR_CHANOPRIVSNEEDED | Non-op tried an operator command |

---

## 8. Team plan — 3 people

Each person **owns** their files so nobody fights over the same code.

### Day 1 — design together, don't split yet
Before anyone writes real logic, agree on the interfaces (the `.hpp` files) as a team:
- What `Server` exposes and how it hands a finished line to a command.
- What's in `Client` — fd, ip, nick, user, realname, buffer, state flags (has-password? registered?).
- What's in `Channel` — members, operators, topic, modes, key, limit.
- How commands dispatch — a `std::map<std::string, ...>`? a switch?
- One shared header for all numeric replies (everyone needs it).

Write those signatures with empty stubs. Now all three can code in parallel against them.

### Person A — the Engine
**Owns:** `Makefile`, `main.cpp`, `Server.hpp` / `Server.cpp`
1. Makefile — rules `NAME all clean fclean re`, flags `-Wall -Wextra -Werror -std=c++98`, no unnecessary relinking.
2. main.cpp — check `argc == 3`, validate port is a number in range, grab password, create the Server.
3. Listening socket — `socket` → `setsockopt(SO_REUSEADDR)` → `bind` → `listen`.
4. Non-blocking — `fcntl(fd, F_SETFL, O_NONBLOCK)`.
5. The single `poll()` loop in `Server::run()` (the heart of the project).
6. `accept()` new clients → set non-blocking → add fd to the pollfd vector → create a Client.
7. On `POLLIN`, `recv()` and hand bytes to B's buffer. On `recv == 0`, disconnect and clean up.
8. Signals — `signal`/`sigaction` on SIGINT sets a flag → break the loop.
9. Cleanup — close every socket, free every Client. No leaks, no crash, ever.

### Person B — Client + parsing + auth + PM
**Owns:** `Client.hpp` / `Client.cpp`, the parser
1. Client class — fd, IP, nick, username, realname, `std::string` buffer, state flags.
2. **Buffer logic** (section 5 above — the hardest bit; passes the `nc` fragmentation test).
3. Parser — split one line into COMMAND + params, handle the `:` trailing param.
4. `PASS` — verify against server password.
5. `NICK` — set/validate, reject duplicates.
6. `USER` — set username + realname.
7. Registration flow — enforce PASS → NICK → USER before anything else, then send `001`.
8. `PRIVMSG` (user → user).
9. Debug console logging (who connected, what command arrived).

> B can unit-test the parser and buffer with fake strings **before A's sockets even run** — no waiting.

### Person C — Channels + broadcast + operators
**Owns:** `Channel.hpp` / `Channel.cpp`, command classes, the replies header
1. Replies header — all `RPL_*` / `ERR_*` in one file (everyone imports it).
2. Channel class — name, topic, members (`Client*` set), operators set, invite list, modes, key, limit.
3. Command dispatch structure (coordinate the mechanism with B).
4. `JOIN` — create if missing, add client, **first joiner = operator**, broadcast the join.
5. Broadcast — send a message to every member of a channel.
6. `PRIVMSG` (→ channel) — forward to all *other* members.
7. Operator commands — `KICK`, `INVITE`, `TOPIC`, `MODE` (i, t, k, o, l).
8. Enforce operator vs regular-user permission checks throughout.

> C can build the Channel data structures + replies standalone too.

---

## 9. Integration order (a sequence, not simultaneous)

1. **A alone** — server accepts one connection and echoes bytes back.
2. **Plug in B** — buffer + parser + auth. Test with `nc`, then a real IRC client connecting.
3. **Plug in C** — `JOIN` + broadcast → two clients talking in a channel.
4. **Operator commands + modes** → full test with the reference client.

---

## 10. Git discipline (this is what saves the project)

- One **feature branch per task**; merge small and often. Don't let a branch live a week.
- **One owner per shared file** (A owns `Server.cpp`). Others request changes through the owner instead of editing directly, or you'll fight constant merge conflicts.
- Pick your **reference client early** (irssi / weechat) — all three test against the **same** one, because that's what the evaluation uses.
- Track with GitHub Issues + a simple kanban (Todo / Doing / Done, one card per task above). Don't over-engineer it.

---

## 11. Testing

The subject's `nc` fragmentation test:
```
nc -C 127.0.0.1 6667
com^Dman^Dd
```
(Ctrl+D sends each piece separately: `com`, then `man`, then `d\n`.) Your buffer must rebuild it into one command `command`.

Then connect a real client:
```
/connect 127.0.0.1 6667 <password>
/nick alice
/join #test
```
Open a second client as `bob`, join `#test`, and confirm messages relay both ways. Test every error path too — wrong password, duplicate nick, non-op trying `KICK`, joining a `+i` channel without an invite, etc.

---

## 12. What you really need to know (the mental models)

- **It's a single-threaded loop.** No fork, no threads. `poll()` never blocks; you never `recv`/`send` outside it.
- **TCP is a stream, not messages.** Hence per-client buffers and newline-based extraction (section 5). This is the #1 thing people get wrong.
- **The server always adds the `nick!user@host` prefix** when relaying — clients never send it.
- **Registration is a gate.** Nothing works until PASS → NICK → USER → `001`.
- **First to JOIN owns the channel.** Operator checks gate KICK/INVITE/TOPIC/MODE.
- **Never crash.** Out of memory, partial data, weird input — handle it. A crash = grade 0.
- **C++98 only.** No `auto`, no range-for, no `nullptr`, no C++11 containers/features. Prefer C++ headers (`<cstring>` over `<string.h>`). No Boost, no external libs.

---

## 13. Resources

- **RFC 1459** — original IRC protocol.
- **RFC 2812** — IRC client protocol (message format, numeric replies). This is your main reference.
- **Modern IRC docs** — modern.ircdocs.horse (readable explanations of commands and replies).
- `man poll`, `man socket`, `man recv`, `man fcntl` — read these.
- Your **reference client's** behavior — connect it to a real server once and watch the raw traffic to see exactly what a correct exchange looks like.

*README reminder (required by the subject):* first line italic — `This project has been created as part of the 42 curriculum by <login1>, <login2>, <login3>.` — plus Description, Instructions, and Resources sections (including how AI was used).