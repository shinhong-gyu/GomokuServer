#include "DBManager.h"

DBManager::DBManager()
{
	conn = mysql_init(NULL);
}

DBManager::~DBManager()
{
	if (conn != NULL)
	{
		mysql_close(conn);
	}
}

bool DBManager::ConnectDB(const char* host, const char* user, const char* pw, const char* db, int port)
{
	if (mysql_real_connect(conn, host, user, pw, db, port, NULL, 0) != NULL)
	{
		std::cout << "[DB] MySQL DB 연결 성공" << "\n";
		return true;
	}

	std::cout << "[DB] MySQL DB 연결 실패" << "\n";
	return false;
}

bool DBManager::Login(const std::string& id, const std::string& pw)
{
	std::string query = "select * from users where id = '" + id + "' and password = '" + pw + "'";

	if (mysql_query(conn, query.c_str()) == 0)
	{
		MYSQL_RES* result = mysql_store_result(conn);

		if (result != NULL && mysql_num_rows(result) == 1)
		{
			mysql_free_result(result);
			return true;
		}
	}

	return false;
}

bool DBManager::UpdateRecord(const std::string& id, bool isWin)
{
	std::string query;

	if (isWin)
	{
		query = "update users set win = win + 1 where id = '" + id + "'";
	}
	else
	{
		query = "update users set lose = lose + 1 where id = '" + id + "'";
	}

	if (mysql_query(conn, query.c_str()) == 0)
	{
		return true;
	}

	std::cout << "[DB] 전적 업데이트 실패" << "\n";

	return false;
}

bool DBManager::GetRecord(const std::string& id, int& win, int& lose)
{
	std::string winQurey = "select win from users where id = '" + id + "'";
	std::string loseQuery = "select lose from users where id = '" + id + "'";

	if (mysql_query(conn, winQurey.c_str()) == 0)
	{
		MYSQL_RES* result = mysql_store_result(conn);

		if (result != nullptr)
		{
			MYSQL_ROW row = mysql_fetch_row(result);

			if (row != nullptr && row[0] != nullptr)
			{
				win = std::stoi(row[0]);
			}

			mysql_free_result(result);
		}
	}
	if (mysql_query(conn, loseQuery.c_str()) == 0)
	{
		MYSQL_RES* result = mysql_store_result(conn);

		if (result != nullptr)
		{
			MYSQL_ROW row = mysql_fetch_row(result);

			if (row != nullptr && row[0] != nullptr)
			{
				lose = std::stoi(row[0]);
			}

			mysql_free_result(result);
		}
	}


	return true;
}
