#include "ServerManager.h"
#include <iostream>
#include <thread>
#include <algorithm>
#include <chrono>

std::mutex mtx;
std::mutex dbMtx;

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
	if (monitorThread.joinable())
	{
		monitorThread.join();
	}

	if (matchingThread.joinable())
	{
		matchingThread.join();
	}

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

	monitorThread = std::thread(&ServerManager::MonitorThread, this);
	matchingThread = std::thread(&ServerManager::MatchMaker, this);

	return true;
}

void ServerManager::AcceptClients()
{
	std::vector<std::thread> clientThreads;

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

					if (loginList.find(loginID) == loginList.end())
					{
						int denyCode = 0;
						std::lock_guard<std::mutex> lock(dbMtx);
						bool isValid = db.Login(loginID, packet.pw, denyCode);

						if (isValid)
						{
							std::cout << "[서버] 로그인 승인: " << loginID << ", 소켓: " << clientSocket << "\n";
							response.x = 1;

							std::cout << "[서버] 새로운 유저가 접속했습니다." << "\n";

							std::shared_ptr<ClientContext> ctx = std::make_shared<ClientContext>();
							ctx->socket = clientSocket;
							ctx->id = loginID;

							clientThreads.emplace_back(&ServerManager::ReceiverThread, this, ctx);

							std::lock_guard<std::mutex> lock(mtx);

							connClients.push_back(ctx);
							idSocketMap[loginID] = clientSocket;
							loginList[loginID] = (DWORD)GetTickCount64();
							onGameList[clientSocket] = true;
						}
						else
						{
							if (denyCode == 1)
							{
								std::cout << "[서버] 로그인 거부 (존재하지 않는 아이디): " << packet.id << "\n";
								response.x = 0;
							}
							else if (denyCode == 2)
							{
								std::cout << "[서버] 로그인 거부 (비밀번호 불일치): " << packet.id << "\n";
								response.x = -2;
							}
						}
					}
					else
					{
						std::cout << "[서버] 이미 로그인 된 아이디: " << packet.id << "\n";
						response.x = -1;
					}

					int result = send(clientSocket, (char*)&response, sizeof(GamePacket), 0);

					if (result == SOCKET_ERROR)
					{
						std::cout << "???" << "\n";
					}
				}

				if (packet.type == PacketType::SIGNIN)
				{
					std::string signInID(packet.id);
					std::string signInPW(packet.pw);

					GamePacket response = {};

					response.type = SIGNIN;

					if (db.SignIn(signInID, signInPW))
					{
						std::cout << "[서버] 회원가입 성공: " << signInID << "\n";
						response.x = 1;
					}
					else
					{
						std::cout << "[서버] 회원가입 실패: " << signInID << "\n";
						response.x = 0;
					}

					int result = send(clientSocket, (char*)&response, sizeof(GamePacket), 0);

					if (result != SOCKET_ERROR)
					{
						std::cout << "회원가입 시도 아이디: " << signInID << ", 결과: " << response.x << "\n";
					}
					else
					{
						std::cout << " 전송 실패" << "\n";
					}
					continue;
				}
			}
		}
	}

	for (auto& thread : clientThreads)
	{
		if (thread.joinable())
		{
			thread.join();
		}
	}
}

void ServerManager::GameRoomThread(std::shared_ptr<ClientContext> p1, std::shared_ptr<ClientContext> p2)
{
	int black = 1;
	int white = 2;

	GameInfo p1Info = {};
	p1Info.color = black;

	{
		std::lock_guard<std::mutex> lock(dbMtx);
		db.GetRecord(p2->id, p1Info.win, p1Info.lose);
	}
	
	strncpy_s(p1Info.oppID, sizeof(p1Info.oppID), p2->id.c_str(), _TRUNCATE);
	p1Info.oppID[sizeof(p1Info.oppID) - 1] = '\0';

	GameInfo p2Info = {};
	p2Info.color = white;
	{
		std::lock_guard<std::mutex> lock(dbMtx);
		db.GetRecord(p1->id, p2Info.win, p2Info.lose);
	}
	

	strncpy_s(p2Info.oppID, sizeof(p2Info.oppID), p1->id.c_str(), _TRUNCATE);
	p2Info.oppID[sizeof(p2Info.oppID) - 1] = '\0';

	int result1 = send(p1->socket, (char*)&p1Info, sizeof(GameInfo), 0);
	int result2 = send(p2->socket, (char*)&p2Info, sizeof(GameInfo), 0);

	std::cout << "[서버 방] 새로운 게임이 시작되었습니다" << std::endl;

	GamePacket packet;

	while (true)
	{
		if (loginList.find(p1->id) == loginList.end() || loginList.find(p2->id) == loginList.end())
		{
			break;
		}

		if (GetPacket(p1, packet))
		{
			if (PacketHandle(p1, p2, packet) == false)
			{
				break;
			}
		}

		if (GetPacket(p2, packet))
		{
			if (PacketHandle(p2, p1, packet) == false)
			{
				break;
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	onGameList[p1->socket] = false;
	onGameList[p2->socket] = false;
	matchInfo.erase(p1->id);
	matchInfo.erase(p2->id);

	std::cout << "[서버 방] 방 닫음" << "\n";
}

void ServerManager::MonitorThread()
{
	while (true)
	{
		DWORD currentTime = (DWORD)GetTickCount64();

		for (auto it = loginList.begin(); it != loginList.end();)
		{
			bool erased = false;
			if (!onGameList[idSocketMap[it->first]])
			{
				if (currentTime - it->second > 15000.0f)
				{
					std::cout << "[서버 - Monitor] 클라이언트 " << it->first << " 연결 끊김 감지" << "\n";

					std::lock_guard<std::mutex> lock(mtx);
					for (auto it2 = connClients.begin(); it2 != connClients.end();)
					{
						if ((*it2)->id == it->first)
						{
							it2 = connClients.erase(it2);
							break;
						}

						++it2;
					}

					onGameList.erase(onGameList.find(idSocketMap[it->first]));
					closesocket(idSocketMap[it->first]);
					it = loginList.erase(it);
					erased = true;
				}
			}

			if (!erased)
			{
				++it;
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
}

void ServerManager::GameFinished(std::shared_ptr<ClientContext> winner, std::shared_ptr<ClientContext> loser)
{
	std::cout << "[서버 방] " + winner->id + " 승리" << "\n";

	loginList[winner->id] = (DWORD)GetTickCount64();
	loginList[loser->id] = (DWORD)GetTickCount64();

	{
		std::lock_guard<std::mutex> lock(dbMtx);
		db.UpdateRecord(loser->id, false);
		db.UpdateRecord(winner->id, true);
	}
}

void ServerManager::MatchMaker()
{
	while (true)
	{
		{
			std::lock_guard<std::mutex> lock(mtx);

			if (connClients.size() >= 2)
			{
				std::shared_ptr<ClientContext> p1 = connClients[0];
				std::shared_ptr<ClientContext> p2 = connClients[1];

				std::cout << "[서버] 매칭 성공! 새로운 방 생성\n";

				std::thread roomThread(&ServerManager::GameRoomThread, this, p1, p2);
				roomThread.detach();

				onGameList[p1->socket] = true;
				onGameList[p2->socket] = true;

				matchInfo[p1->id] = p2;
				matchInfo[p2->id] = p1;

				connClients.erase(connClients.begin(), connClients.begin() + 2);
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void ServerManager::ReceiverThread(std::shared_ptr<ClientContext> ctx)
{
	GamePacket packet;

	while (true)
	{
		int result = recv(ctx->socket, (char*)&packet, sizeof(GamePacket), 0);

		{
			std::lock_guard<std::mutex> lock(mtx);
			if (result <= 0)
			{
				std::cout << "[서버 - Recv] 클라이언트 " << ctx->id << " 연결 끊김 감지" << "\n";

				onGameList[ctx->socket] = false;

				if (matchInfo[ctx->id])
				{
					auto opponent = matchInfo[ctx->id];

					if (matchInfo.count(opponent->id) > 0 && matchInfo[opponent->id]->id == ctx->id)
					{
						packet.type = PacketType::LEAVE;

						send(matchInfo[ctx->id]->socket, (char*)&packet, sizeof(GamePacket), 0);

						onGameList[matchInfo[ctx->id]->socket] = false;

						matchInfo.erase(opponent->id);
					}

					matchInfo.erase(ctx->id);
				}

				break;
			}

			loginList[ctx->id] = (DWORD)GetTickCount64();

			if (packet.type == PacketType::HEARTBEAT)
			{
				continue;
			}

			if (packet.type == PacketType::MATCHING)
			{
				connClients.push_back(ctx);
				onGameList[ctx->socket] = true;
				continue;
			}
		}


		{
			std::lock_guard<std::mutex> lock(ctx->queueMtx);
			ctx->packetQueue.push(packet);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

bool ServerManager::GetPacket(std::shared_ptr<ClientContext> ctx, GamePacket& packet)
{
	std::lock_guard<std::mutex> lock(ctx->queueMtx);

	if (ctx->packetQueue.empty())
	{
		return false;
	}

	packet = ctx->packetQueue.front();
	ctx->packetQueue.pop();
	return true;
}

bool ServerManager::PacketHandle(std::shared_ptr<ClientContext> sender, std::shared_ptr<ClientContext> receiver, GamePacket& packet)
{
	int result = send(receiver->socket, (char*)&packet, sizeof(GamePacket), 0);

	std::string senderID = sender->id;
	std::string receiverID = receiver->id;

	if (result == SOCKET_ERROR)
	{
		std::cout << "[서버 - Send] 클라이언트 " << receiver->id << " 연결 끊김 감지" << "\n";

		packet.type = PacketType::LEAVE;

		send(sender->socket, (char*)&packet, sizeof(GamePacket), 0);

		return false;
	}

	if (packet.type == WIN || packet.type == LEAVE)
	{
		if (packet.type == WIN)
		{
			GameFinished(sender, receiver);
		}

		return false;
	}

	return true;
}
