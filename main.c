#include "switchboard.h"
#include "telegram.h"

void processMsg(SOCKET csocket, const char* buff, sqlite3* sql, const char* email, const char* friendlyName);

int main(void) {
	WSADATA wsaData = { 0 };
	int iResult, iSendResult = 0;
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("wsastartup failed: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	struct addrinfo* result = NULL;
	struct addrinfo hints = { 0 };

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	iResult = getaddrinfo(NULL, "1863", &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	SOCKET lsocket = INVALID_SOCKET;
	lsocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (lsocket == INVALID_SOCKET) {
		printf("socket error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	iResult = bind(lsocket, result->ai_addr, (int)result->ai_addrlen);
	freeaddrinfo(result);
	if (iResult == SOCKET_ERROR) {
		printf("bind error: %ld\n", WSAGetLastError());
		closesocket(lsocket);
		WSACleanup();
		return 1;
	}
	printf("notification server opened\n");
	if (listen(lsocket, SOMAXCONN) == SOCKET_ERROR) {
		printf("listen failed: %ld", WSAGetLastError());
		closesocket(lsocket);
		WSACleanup();
		return 1;
	}

	// sql
	sqlite3* db = NULL;
	if (sqlite3_open("msn.db", &db) != SQLITE_OK) {
		printf("sql fail\n");
		closesocket(lsocket);
		WSACleanup();
	}
	sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS telegram(id INTEGER PRIMARY KEY, firstName TEXT NOT NULL);", NULL, NULL, NULL);
	sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS matrix(id TEXT PRIMARY KEY);", NULL, NULL, NULL);

	// config.ini
	char path[MAX_PATH];
	char email[128];
	char friendlyName[128];
	GetModuleFileNameA(NULL, path, MAX_PATH);
	PathRemoveFileSpecA(path);
	strcat_s(path, MAX_PATH, "\\config.ini");
	GetPrivateProfileStringA("MSN", "email", "localhost@hotmail.com", email, sizeof(email), path);
	GetPrivateProfileStringA("MSN", "friendlyName", "Edit%20Name%20In%20Config.ini", friendlyName, sizeof(friendlyName), path);
	startBot();
	setSql(db);
	CreateThread(NULL, 0, update, NULL, 0, NULL);

	// main cycle
	char* buff = malloc(sizeof(char) * 8192);
	if (buff == NULL) {
		printf("malloc failed\n");
		closesocket(lsocket);
		WSACleanup();
		return 1;
	}

	SOCKET csocket = INVALID_SOCKET;

	do {
		csocket = accept(lsocket, NULL, NULL);
		if (csocket == INVALID_SOCKET) {
			printf("accept error: %ld", WSAGetLastError());
			closesocket(csocket);
			WSACleanup();
			return 1;
		}
		printf("connected to notification server\n");
		setNSLive(true);
		setNotificationSocket(csocket);
		memset(buff, 0, 8192);
		while (1) {
			memset(buff, 0, 8192);
			iResult = recv(csocket, buff, 8192, 0);
			if (iResult > 0) {
				processMsg(csocket, buff, db, email, friendlyName);
			}
			else if (iResult == 0) {
				printf("closing\n");
				break;
			}
			else {
				printf("recv fail: %ld", WSAGetLastError());
				break;
			}
		}
		printf("connection closed\n");
	} while (1);
	WSACleanup();
	free(buff);
	return 0;
}

void processMsg(SOCKET csocket, const char* buff, sqlite3* sql, const char* email, const char* friendlyName) {
	char sendBuff[512];
	int trId = 0;
	if (strstr(buff, "VER") != NULL) {
		sscanf(strstr(buff, "VER"), "VER %d", &trId);
		snprintf(sendBuff, 512, "VER %d MSNP2\r\n", trId);
		send(csocket, sendBuff, strlen(sendBuff), 0);
	}
	else if (strstr(buff, "INF") != NULL) {
		sscanf(strstr(buff, "INF"), "INF %d", &trId);
		snprintf(sendBuff, 512, "INF %d MD5\r\n", trId);
		send(csocket, sendBuff, strlen(sendBuff), 0);
	}
	else if (strstr(buff, "USR") != NULL) {
		char securityPackage[16];
		char whatToDo[16];
		sscanf(strstr(buff, "USR"), "USR %d %s %s", &trId, securityPackage, whatToDo);
		if (strncmp(whatToDo, "I", 1) == 0) {
			snprintf(sendBuff, 512, "USR %d %s S 12345\r\n", trId, securityPackage);
		}
		else if (strncmp(whatToDo, "S", 1) == 0) {
			snprintf(sendBuff, 512, "USR %d OK %s %s\r\n", trId, email, friendlyName);
		}
		send(csocket, sendBuff, strlen(sendBuff), 0);
	}
	else if (strstr(buff, "SYN") != NULL) {
		sscanf(strstr(buff, "SYN"), "SYN %d", &trId);
		snprintf(sendBuff, 512, "SYN %d 0\r\n", trId);
		send(csocket, sendBuff, strlen(sendBuff), 0);
		// gtc
		snprintf(sendBuff, 512, "GTC %d 0 A\r\n", trId);
		send(csocket, sendBuff, strlen(sendBuff), 0);
		// bl
		snprintf(sendBuff, 512, "BLP %d 0 AL\r\n", trId);
		send(csocket, sendBuff, strlen(sendBuff), 0);
		// sql part
		sqlite3_stmt* stmt = NULL;
		int rc = 0;
		int totalCount = 0;
		rc = sqlite3_prepare_v2(sql, "SELECT COUNT(*) FROM msn", -1, &stmt, NULL);
		sqlite3_step(stmt);
		totalCount = sqlite3_column_int(stmt, 0);
		rc = sqlite3_prepare_v2(sql, "SELECT * FROM msn", -1, &stmt, NULL);
		int count = 0;
		while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
			count++;
			snprintf(sendBuff, 512, "LST %d FL 0 %d %d %lld@hotmail.com %s\r\n", trId, count, totalCount, sqlite3_column_int64(stmt, 0), sqlite3_column_text(stmt, 1));
			send(csocket, sendBuff, strlen(sendBuff), 0);
			snprintf(sendBuff, 512, "NLN NLN %lld@hotmail.com %s\r\n", sqlite3_column_int64(stmt, 0), sqlite3_column_text(stmt, 1));
			send(csocket, sendBuff, strlen(sendBuff), 0);
		}
		sqlite3_finalize(stmt);
		snprintf(sendBuff, 512, "LST %d AL 0 0 0\r\n", trId);
		send(csocket, sendBuff, strlen(sendBuff), 0);
		snprintf(sendBuff, 512, "LST %d BL 0 0 0\r\n", trId);
		send(csocket, sendBuff, strlen(sendBuff), 0);
		snprintf(sendBuff, 512, "LST %d RL 0 0 0\r\n", trId);
		send(csocket, sendBuff, strlen(sendBuff), 0);
	}
	else if (strstr(buff, "CHG") != NULL) {
		char status[4];
		sscanf(strstr(buff, "CHG"), "CHG %d %s", &trId, status);
		snprintf(sendBuff, 512, "CHG %d %s\r\n", trId, status);
		send(csocket, sendBuff, strlen(sendBuff), 0);
	}
	else if (strstr(buff, "CVR") != NULL) {
		sscanf(strstr(buff, "CVR"), "CVR %d", &trId);
		snprintf(sendBuff, 512, "CVR %d 1.0.0863 1.0.0863 1.0.0863 http://localhost http://localhost\r\n", trId);
		send(csocket, sendBuff, strlen(sendBuff), 0);
	}
	else if (strstr(buff, "SND") != NULL) {
		sscanf(strstr(buff, "SND"), "SND %d", &trId);
		snprintf(sendBuff, 512, "SND %d OK\r\n", trId);
		send(csocket, sendBuff, strlen(sendBuff), 0);
	}
	else if (strstr(buff, "ADD") != NULL) {
		char listType[3];
		char nemail[128];
		char nname[128];
		sscanf(strstr(buff, "ADD"), "ADD %d %s %s %s", &trId, listType, nemail, nname);
		snprintf(sendBuff, 512, "ADD %d %s 0 %s %s\r\n", trId, listType, nemail, nname);
		send(csocket, sendBuff, strlen(sendBuff), 0);
		snprintf(sendBuff, 512, "NLN NLN %s %s\r\n", nemail, nname);
		send(csocket, sendBuff, strlen(sendBuff), 0);
		
	}
	else if (strstr(buff, "REA") != NULL) {
		char nemail[128];
		char nname[128];
		sscanf(strstr(buff, "REA"), "REA %d %s %s", &trId, nemail, nname);
		snprintf(sendBuff, 512, "REA %d %s %s\r\n", trId, nemail, nname);
		send(csocket, sendBuff, strlen(sendBuff), 0);
	}
	else if (strstr(buff, "REM") != NULL) {
		char listType[3];
		char nemail[128];
		sscanf(strstr(buff, "REM"), "REM %d %s %s", &trId, listType, nemail);
		snprintf(sendBuff, 512, "REM %d %s 0 %s\r\n", trId, listType, nemail);
		send(csocket, sendBuff, strlen(sendBuff), 0);
	}
	else if (strstr(buff, "XFR") != NULL) {
		u_short port = addPortAndGet();
		sscanf(strstr(buff, "XFR"), "XFR %d", &trId);
		snprintf(sendBuff, 512, "XFR %d SB 127.0.0.1:%d CKI 12345\r\n", trId, port);
		CreateThread(NULL, 0, startSwitchboard, (LPVOID)(DWORD_PTR)port, 0, NULL);
		send(csocket, sendBuff, strlen(sendBuff), 0);
	}
	else if (strstr(buff, "URL") != NULL) {
		char theThing[16];
		char nemail[128];
		sscanf(strstr(buff, "URL"), "URL %d %s", &trId, theThing);
		if (strncmp(theThing, "INBOX", 5) == 0) {			
			snprintf(sendBuff, 512, "URL %d /mail https://outlook.office.com 0\r\n", trId);
		}
		else if (strncmp(theThing, "COMPOSE", 7) == 0) {
			sscanf(strstr(buff, "URL"), "URL %d %s %s", &trId, theThing, nemail);
			snprintf(sendBuff, 512, "URL %d /mail/%s https://outlook.office.com 0\r\n", trId, nemail);
		}
		send(csocket, sendBuff, strlen(sendBuff), 0);
	}
}