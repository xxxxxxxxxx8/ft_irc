#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>


class Server{
	public:
	server(int poer , const std:;string &password);
	~server();

	void setup();

	private:
	server(const Server &other); //not implemnt on purpos
	server &operator=(const Server &other); //not implemnt on purpos

	int _port;
	std::string _password;
	int _socket;
};


#endif