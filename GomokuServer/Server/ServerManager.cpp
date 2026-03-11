#include "ServerManager.h"
#include <iostream>
#include <thread>
#include <algorithm>
#include <chrono>

ServerManager::ServerManager()
{
	WSADATA wsaData;

	int a = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (a)
	{
		std::cout << "[서버] WSAStartup 실패: " << a << "\n";
	}

	serverSocket = INVALID_SOCKET;
}

ServerManager::~ServerManager()
{
	closesocket(serverSocket);
	WSACleanup();
}

bool ServerManager::StartServer(int port)
{
	if (!db.ConnectDB("localhost", "root", "1234", "GomokuDB", 3306))
	{
		std::cout << "[서버] DB 연결 실패" << "\n";
		return false;
	}

	serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (serverSocket == INVALID_SOCKET)
	{
		return false;
	}

	SOCKADDR_IN serverAddr = {};
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		return false;
	}

	if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		return false;
	}

	std::cout << "[서버] DB 연결 성공, " << port << "번 포트에서 접속 대기 중..." << "\n";

	return true;
}

void ServerManager::AcceptClients()
{
	std::thread monitorThread(&ServerManager::MonitorHeartbeats, this);

	monitorThread.detach();

	while (true)
	{
		sockaddr_in  clientAddr;
		int addrLen = sizeof(clientAddr);

		SOCKET clientSocket = accept(serverSocket, (SOCKADDR*)&clientAddr, &addrLen);

		if (clientSocket == INVALID_SOCKET)
		{
			int err = WSAGetLastError();
			std::cout << "Accept failed with error: " << err << std::endl;
			break;
		}

		if (clientSocket != INVALID_SOCKET)
		{
			GamePacket packet;

			if (recv(clientSocket, (char*)&packet, sizeof(GamePacket), 0) > 0)
			{
				if (packet.type == LOGIN)
				{
					std::string loginID(packet.id);

					GamePacket response = {};
					response.type = LOGIN;

					if (loginList[loginID] == 0)
					{
						bool isValid = db.Login(loginID, packet.pw);

						if (isValid)
						{
							std::cout << "[서버] 로그인 승인: " << loginID << "\n";
							response.x = 1;

							std::cout << "[서버] 새로운 유저가 접속했습니다." << "\n";

							//std::thread hbThread(&ServerManager::HeartbeatThread, this, clientSocket);

							//	hbThread.detach();


							connClients.push_back(std::make_pair(clientSocket, loginID));
							idSocketMap[loginID] = clientSocket;
							loginList[loginID] = GetTickCount64();
							onGameList[clientSocket] = true;
						}
						else
						{
							std::cout << "[서버] 로그인 거부: " << packet.id << "\n";
							response.x = 0;
						}
					}
					else
					{
						std::cout << "[서버] 이미 로그인 된 아이디: " << packet.id << "\n";
						response.x = -1;
					}

					send(clientSocket, (char*)&response, sizeof(GamePacket), 0);
				}
			}

			if (connClients.size() >= 2)
			{
				SOCKET p1 = connClients[0].first;
				SOCKET p2 = connClients[1].first;
				std::string p1ID = connClients[0].second;
				std::string p2ID = connClients[1].second;

				std::cout << "[서버] 2명이 모여 새로운 게임 방을 생성합니다. \n";

				std::thread roomThread(&ServerManager::GameRoomThread, this, p1, p2, p1ID, p2ID);

				roomThread.detach();

				connClients.erase(connClients.begin());
				connClients.erase(connClients.begin());
			}
		}
	}
}

void ServerManager::GameRoomThread(SOCKET player1, SOCKET player2, std::string p1ID, std::string p2ID)
{
	int black = 1;
	int white = 2;

	GameInfo p1Info = {};
	p1Info.color = black;
	db.GetRecord(p2ID, p1Info.win, p1Info.lose);

	strncpy_s(p1Info.oppID, sizeof(p1Info.oppID), p2ID.c_str(), _TRUNCATE);
	p1Info.oppID[sizeof(p1Info.oppID) - 1] = '\0';

	GameInfo p2Info = {};
	p2Info.color = white;
	db.GetRecord(p1ID, p2Info.win, p2Info.lose);

	strncpy_s(p2Info.oppID, sizeof(p2Info.oppID), p1ID.c_str(), _TRUNCATE);
	p2Info.oppID[sizeof(p2Info.oppID) - 1] = '\0';

	send(player1, (char*)&p1Info, sizeof(GameInfo), 0);
	send(player2, (char*)&p2Info, sizeof(GameInfo), 0);

	std::cout << "[서버 방] 새로운 게임이 시작되었습니다" << std::endl;

	GamePacket packet;

	int result;

	while (true)
	{
		result = recv(player1, (char*)&packet, sizeof(GamePacket), 0);

		if (result <= 0)
		{
			std::cout << "[서버 방] Player 1 연결 끊김. 게임 종료" << "\n";

			packet.type = PacketType::LEAVE;

			send(player2, (char*)&packet, sizeof(GamePacket), 0);

			break;
		}

		loginList[p1ID] = GetTickCount64();

		send(player2, (char*)&packet, sizeof(GamePacket), 0);

		if (packet.type == PacketType::WIN)
		{
			std::cout << "[서버 방] " + p1ID + " 승리" << "\n";

			loginList[p1ID] = GetTickCount64();
			loginList[p2ID] = GetTickCount64();

			db.UpdateRecord(p1ID, true);
			db.UpdateRecord(p2ID, false);

			break;
		}

		result = recv(player2, (char*)&packet, sizeof(GamePacket), 0);

		loginList[p2ID] = GetTickCount64();

		if (result <= 0)
		{
			std::cout << "[서버 방] Player 2 연결 끊김. 게임 종료" << "\n";

			packet.type = PacketType::LEAVE;

			send(player1, (char*)&packet, sizeof(GamePacket), 0);

			break;
		}

		send(player1, (char*)&packet, sizeof(GamePacket), 0);

		if (packet.type == PacketType::WIN)
		{
			std::cout << "[서버 방] " + p2ID + " 승리" << "\n";

			loginList[p1ID] = GetTickCount64();
			loginList[p2ID] = GetTickCount64();

			db.UpdateRecord(p1ID, false);
			db.UpdateRecord(p2ID, true);

			break;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	onGameList[player1] = false;
	onGameList[player2] = false;

	std::cout << "[서버 방] 방 닫음" << "\n";
}

void ServerManager::MonitorHeartbeats()
{
	while (true)
	{
		DWORD currentTime = GetTickCount64();

		for (auto it = loginList.begin(); it != loginList.end();)
		{

			if (!onGameList[idSocketMap[it->first]])
			{

				if (currentTime > it->second && currentTime - it->second > 30000.0f)
				{
					std::cout << "\n[서버] 클라이언트 " << it->first << " 연결 끊김 감지" << "\n";

					for (auto it2 = connClients.begin(); it2 != connClients.end();)
					{
						if (it2->first == idSocketMap[it->first])
						{
							it2 = connClients.erase(it2);
							break;
						}

						++it2;
					}

					onGameList.erase(onGameList.find(idSocketMap[it->first]));
					closesocket(idSocketMap[it->first]);
					it = loginList.erase(it);
				}
				else
				{
					++it;
				}
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
}