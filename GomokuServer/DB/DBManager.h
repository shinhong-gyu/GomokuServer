#pragma once

#include <mysql.h>
#include <string>
#include <iostream>

class DBManager
{
public:
	DBManager();
	~DBManager();

	bool ConnectDB(const char* host, const char* user, const char* pw, const char* db, int port);

	bool Login(const std::string& id, const std::string& pw);

	bool UpdateRecord(const std::string& id, bool isWin);

	bool GetRecord(const std::string& id, int& win, int& lose);
private:
	MYSQL* conn;
};