#pragma once

#pragma comment(lib, "ws2_32.lib")

#include <winsock2.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <queue>

#include "../DB/DBManager.h"


enum PacketType { LOGIN = 1, STONE = 2, WIN = 3, LEAVE = 4, HEARTBEAT = 5, MATCHING = 6 , SIGNIN = 7};

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

struct ClientContext
{
	SOCKET socket = INVALID_SOCKET;
	std::string id;
	std::queue<GamePacket> packetQueue;
	std::mutex queueMtx;
};


class ServerManager
{
public:
	ServerManager();
	~ServerManager();

	bool StartServer(int port);

	void AcceptClients();

	void GameRoomThread(std::shared_ptr<ClientContext> p1, std::shared_ptr<ClientContext> p2);

	void MonitorHeartbeats();

	void GameFinished(std::shared_ptr<ClientContext> winner, std::shared_ptr<ClientContext> loser);

	void MatchMaker();

	void ReceiverThread(std::shared_ptr<ClientContext> ctx);

	bool GetPacket(std::shared_ptr<ClientContext> ctx, GamePacket& packet);

	bool PacketHandle(std::shared_ptr<ClientContext> sender, std::shared_ptr<ClientContext> receiver, GamePacket& packet);

private:
	SOCKET serverSocket;


	DBManager db;

	std::vector<std::shared_ptr<ClientContext>> connClients;

	std::unordered_map<std::string, DWORD> loginList;
	std::unordered_map<SOCKET, bool> onGameList;
	std::unordered_map<std::string, SOCKET> idSocketMap;

	std::unordered_map<std::string, std::shared_ptr<ClientContext>> matchInfo;
};
