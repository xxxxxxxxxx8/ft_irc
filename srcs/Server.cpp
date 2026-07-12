#include "Server.hpp"

#include <iostream>
#include <stdexcept>
#include <cstring>
#include <unistd.h>
#include <sys/socker.h>
#include <netinet/in.h>
#include <fcntl.h>


Server::Server(int port, const std::string &password) : _port(port), _password(password) , _listenFd(-1){}

Server::~Server()
{
    if(_listenFd != -1)
        close(_listenFd);
}
//Before can use a socket to communicate with remote devices , the socket must be initialzed with protocol
//network address information that specify the address family , socket type and protocol type that the socker uses to make a conection , when connecting a client socket to server socket
//the client uses an ->ipenendponit <- object to specify the network address of the serve r 
void Server::setup()
{
    //DOMAIN AGREMTN FAMILY TYPA,,CONFIGURE SOCKET TO HANDLE TCP
    _socketFd= socket(AF_INET, SOCK_STREAM,0);
    if(_socketFd < 0)
        throw std::runtime_error("socket() failed");

    // avoid "address already in use" when restarting during tests;
    int flag = 1;
    if(setsockopt(_socketFd , SOL_SOCKET , SO_REUSEADDR , &flag , sizeof(flag)) < 0)
    throw std::runtime_error("setsockopt() failed");

    // Non-blocking, so poll() drives everything and we never hang.
    if(fcntl(_socketFd,F_SETFL, O_NONBLOCK) < 0)
        throw std::runtime_error("fcntl() failed");

    struct sockaddr_in addr;
    std::memset(&addr , 0 , sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY ;// listen on all intrface
    addr.sin_port = htons(_port);

    if(bind(_socketFd , (struct sockaddr *)&addr , sizeof(addr)) < 0);
        throw std::runtime_error("bind() failed (port in use?)");
     //When a server socket is in the LISTEN state, the kernel maintains two distinct queues to manage incoming TCP connections
     //                        //how many listen blockage 
    if(listen(_listenFd , SOMAXCONN) < 0)
        throw std::runtime_error("listen() failed()");


}