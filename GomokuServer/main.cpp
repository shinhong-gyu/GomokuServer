#include "Server/ServerManager.h"

#include <iostream>

int main()
{
	ServerManager server;

	if (server.StartServer(9000))
	{
		server.AcceptClients();
	}
	else
	{
		std::cout << "서버 열기 실패" << std::endl;
	}

	return 0;
}