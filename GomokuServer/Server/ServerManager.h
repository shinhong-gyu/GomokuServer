#pragma once
#include <winsock2.h>
#include <vector>
#include <string>
#include <unordered_map>

#include "../DB/DBManager.h"

#pragma comment(lib, "ws2_32.lib")

enum PacketType { LOGIN = 1, STONE = 2, WIN = 3, LEAVE = 4, HEARTBEAT = 5 };

struct GamePacket
{
	int type;
	int x, y;
	char id[20];
	char pw[50];
};

struct GameInfo
{
	char oppID[20];
	int win;
	int lose;
	int color;
};

struct ClientInfo {
	SOCKET sock;
	std::string id;
	DWORD lastHeartbeatTime;
};

class ServerManager
{
public:
	ServerManager();
	~ServerManager();

	bool StartServer(int port);

	void AcceptClients();

	void GameRoomThread(SOCKET player1, SOCKET player2, std::string p1ID, std::string p2ID);

	void MonitorHeartbeats();

	/*	void HeartbeatThread(SOCKET client);*/
private:
	SOCKET serverSocket;

	std::vector<std::pair<SOCKET, std::string>> connClients;

	DBManager db;

	std::unordered_map<std::string, DWORD> loginList;
	std::unordered_map<SOCKET, bool> onGameList;
	std::unordered_map<std::string, SOCKET> idSocketMap;
};
