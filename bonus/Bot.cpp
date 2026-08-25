#include "Bot.hpp"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdlib>
#include <sstream>
#include <cstring>
#include <netdb.h>
#include <fstream>

IrcBot::IrcBot(std::string ip, int p, std::string pwd) 
    : _serverIp(ip), _port(p), _password(pwd), _nickname("irc_bot") {
}

void IrcBot::sendToServer(const std::string& msg) {
    std::string packet = msg + "\r\n";
    send(_socket, packet.c_str(), packet.length(), 0);
}

void IrcBot::handleBotCommand(const std::string& senderNick, const std::string& text) {
    if (text == ":!help") {
        sendToServer("PRIVMSG " + senderNick + " :ask c3ph1r or Not_kilox or am1ne '_'");
    } 
    else if (text == ":!file") {
        sendDccFile(senderNick);
    }
}

void IrcBot::sendDccFile(const std::string& receiverNick) {
    std::string filename = "bot_gift.txt";
    std::ofstream file(filename.c_str());
    file << "Hello! This file was sent directly from the bot via Peer-to-Peer DCC.\n";
    file.close();

    int dccSocket = socket(AF_INET, SOCK_STREAM, 0);
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = 0;
    
    bind(dccSocket, (struct sockaddr*)&addr, sizeof(addr));
    listen(dccSocket, 1);
    
    socklen_t len = sizeof(addr);
    getsockname(dccSocket, (struct sockaddr*)&addr, &len);
    int myPort = ntohs(addr.sin_port);
    
    unsigned long ipAsInt = 2130706433; 
    
    std::ostringstream ctcp;
    ctcp << "PRIVMSG " << receiverNick << " :\001DCC SEND " 
         << filename << " " << ipAsInt << " " << myPort << " 74\001";
    
    sendToServer(ctcp.str());

    int clientSocket = accept(dccSocket, NULL, NULL);
    if (clientSocket >= 0) {
        std::ifstream infile(filename.c_str());
        std::string content((std::istreambuf_iterator<char>(infile)), 
                             std::istreambuf_iterator<char>());
        send(clientSocket, content.c_str(), content.length(), 0);
        close(clientSocket);
    }
    close(dccSocket);
}

void IrcBot::run() {
    _socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(_port);

    struct hostent *host = gethostbyname(_serverIp.c_str());
    if (host == NULL) {
        std::cerr << "Error: Host not found" << std::endl;
        return;
    }
    std::memcpy(&serv_addr.sin_addr.s_addr, host->h_addr, host->h_length);

    if (connect(_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Error: Cannot connect to server! Is ircserv running on port " << _port << "?" << std::endl;
        return;
    }

    std::cout << "Bot connected successfully!" << std::endl;

    sendToServer("PASS " + _password);
    sendToServer("NICK " + _nickname);
    sendToServer("USER bot 0 * :I am a Bot");

    char buffer[4096];
    std::string dataBuffer = "";
    
    while (true) {
        int bytesRead = recv(_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) {
            std::cerr << "Disconnected from server." << std::endl;
            break;
        }
        buffer[bytesRead] = '\0';
        
        dataBuffer += buffer;
        
        size_t pos;
        while ((pos = dataBuffer.find("\r\n")) != std::string::npos) {
            std::string line = dataBuffer.substr(0, pos);
            dataBuffer.erase(0, pos + 2);
            
            if (line.substr(0, 4) == "PING") {
                sendToServer("PONG " + line.substr(5));
                continue;
            }
            
            std::istringstream iss(line);
            std::string prefix, command, target, text;
            iss >> prefix >> command >> target >> text;

            if (command == "PRIVMSG" && target == _nickname) {
                size_t exclam = prefix.find('!');
                if (exclam != std::string::npos) {
                    std::string senderNick = prefix.substr(1, exclam - 1);
                    handleBotCommand(senderNick, text);
                }
            }
        }
    }
    close(_socket);
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "Usage: ./bot <server_ip> <port> <password>" << std::endl;
        return 1;
    }
    IrcBot myBot(argv[1], atoi(argv[2]), argv[3]);
    myBot.run();
    return 0;
}
