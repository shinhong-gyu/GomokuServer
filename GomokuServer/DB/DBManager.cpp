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

bool DBManager::Login(const std::string& id, const std::string& pw, int& error)
{
	std::string query = "select password from accountinfo where id = '" + id + "'";

	if (mysql_query(conn, query.c_str()) != 0)
	{
		std::cout << "[DB 에러] " << mysql_error(conn) << "\n";
		error = -1;
		return false;
	}


	MYSQL_RES* result = mysql_store_result(conn);

	if (result == NULL || mysql_num_rows(result) == 0)
	{
		if (result)
		{
			mysql_free_result(result);
		}
		error = 1;
		return false;
	}

	MYSQL_ROW row = mysql_fetch_row(result);
	std::string db_pw = row[0];
	mysql_free_result(result);

	if (db_pw == pw)
	{
		error = 0;
		return true;
	}

	error = 2;
	return false;

}

bool DBManager::UpdateRecord(const std::string& id, bool isWin)
{
	std::string query;

	if (isWin)
	{
		query = "update winloseinfo set win = win + 1 where id = '" + id + "'";
	}
	else
	{
		query = "update winloseinfo set lose = lose + 1 where id = '" + id + "'";
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
	std::string winQurey = "select win from winloseinfo where id = '" + id + "'";
	std::string loseQuery = "select lose from winloseinfo where id = '" + id + "'";

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

bool DBManager::SignIn(const std::string& id, const std::string& pw)
{
	if (mysql_query(conn, "start transaction") != 0)
	{
		return false;
	}

	std::string query1 = "insert into accountinfo (id, password) values ('" + id + "', '" + pw + "')";

	if (mysql_query(conn, query1.c_str()) != 0)
	{
		mysql_query(conn, "rollback"); 
		return false;
	}

	std::string query2 = "insert into winloseinfo (id, win, lose) values ('" + id + "', 0, 0)";

	if (mysql_query(conn, query2.c_str()) != 0) 
	{
		mysql_query(conn, "rollback");
		return false;
	}

	if (mysql_query(conn, "commit") == 0)
	{
		return true;
	}

	mysql_query(conn, "rollback");
	return false;
}
