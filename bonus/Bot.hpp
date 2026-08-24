#ifndef BOT_HPP
#define BOT_HPP

#include <string>

class IrcBot {
private:
    std::string _serverIp;
    int _port;
    std::string _password;
    std::string _nickname;
    int _socket;

    void sendToServer(const std::string& msg);
    void handleBotCommand(const std::string& senderNick, const std::string& text);
    void sendDccFile(const std::string& receiverNick);

public:
    IrcBot(std::string ip, int p, std::string pwd);
    void run();
};

#endif
