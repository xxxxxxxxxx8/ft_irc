#include <iostream>
#include <string>
#include <cstdlib>
#include <cctype>

static bool parsePort(const std::string &str ,int &port)
{
    if(str.empty() || str.length() > 5)
        return false;
    for(size_t i = 0 ; i < str.length() ; ++i)
    {
        if(!std::isdigit(static_cast<unsigned int>(str[i])))
            return false;
    }
    port = std::atoi(str.c_str());
    return (port >= 1024 && port <= 65535);
}

int main(int argc , char *argv[])
{
    if(argc != 3)
    {
        std::cerr <<"Error: should Usage: ./program <port> <number> : try again() dont be lazy" << std::endl;
        return 1;
    }
    int port = 0;
    if(!parsePort(argv[1],port))
    {
        std::cerr << "Error: port myst be a number betwen 1024 and 65535 i recomendd use 1337school"<<std::endl;
        return 1;
    }
    
    std::string password(argv[2]);
    if(password.empty())
    {
        std::cerr << "Error password cannot be empty use like CRITICAL" << std::endl;
        return 1;
    }
    std::cout << "[server] port :" << port << std::endl;
    std::cout << "[server] password :" << password << std::endl;

    return 0;
}