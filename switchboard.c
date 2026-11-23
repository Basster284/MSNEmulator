#include "switchboard.h"
#include "telegram.h"

void processBoard(SOCKET csocket, char* buff, long long* id, SwitchBoardData* data, const char* email, const char* friendlyName);

DWORD WINAPI startSwitchboard(LPVOID lpParam) {
	u_short port = (u_short)(DWORD_PTR)lpParam;
	int iResult, iSendResult = 0;
	struct addrinfo* result = NULL;
	struct addrinfo hints = { 0 };

	long long id = 0;
	SwitchBoardData data = { 0 };
	MatrixSwitchBoardData mData = { 0 };

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	char portStr[7];
	snprintf(portStr, 7, "%d", port);
	iResult = getaddrinfo(NULL, portStr, &hints, &result);

	SOCKET lsocket = INVALID_SOCKET;
	lsocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (lsocket == INVALID_SOCKET) {
		printf("socket error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		ExitThread(0);
	}

	int opt = 1;
	setsockopt(lsocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

	iResult = bind(lsocket, result->ai_addr, (int)result->ai_addrlen);
	freeaddrinfo(result);
	if (iResult == SOCKET_ERROR) {
		printf("bind error: %ld\n", WSAGetLastError());
		closesocket(lsocket);
		ExitThread(0);
	}

	printf("opened switchboard on port %d\n", port);
	if (listen(lsocket, SOMAXCONN) == SOCKET_ERROR) {
		printf("listen failed: %ld", WSAGetLastError());
		closesocket(lsocket);
		ExitThread(0);
	}

	SOCKET csocket = INVALID_SOCKET;

	csocket = accept(lsocket, NULL, NULL);
	closesocket(lsocket);
	if (csocket == INVALID_SOCKET) {
		printf("accept error: %ld", WSAGetLastError());
		closesocket(csocket);
		ExitThread(0);
	}

	printf("client connected\n");

	char path[MAX_PATH];
	char email[128];
	char friendlyName[128];
	GetModuleFileNameA(NULL, path, MAX_PATH);
	PathRemoveFileSpecA(path);
	strcat_s(path, MAX_PATH, "\\config.ini");
	GetPrivateProfileStringA("MSN", "email", "localhost@hotmail.com", email, sizeof(email), path);
	GetPrivateProfileStringA("MSN", "friendlyName", "Edit name in config.ini", friendlyName, sizeof(friendlyName), path);

	// main cycle
	char* buff = malloc(sizeof(char) * 8192);
	if (buff == NULL) {
		printf("malloc failed\n");
		closesocket(csocket);
		ExitThread(0);
	}
	do {
		memset(buff, 0, 8192);
		iResult = recv(csocket, buff, 8192, 0);
		if (iResult > 0) {
			processBoard(csocket, buff, &id, &data, email, friendlyName);
		}
		else if (iResult == 0) {
			printf("closing switchboard\n");
			break;
		}
		else {
			printf("recv fail: %ld", WSAGetLastError());
			break;
		}
	} while (iResult > 0);
	free(buff);
	closesocket(csocket);
	deleteSwitchboardSocket(data);
	ExitThread(0);
}

void processBoard(SOCKET csocket, char* buff, long long* id, SwitchBoardData* data, const char* email, const char* friendlyName) {
	char sendBuff[512];
	int trId = 0;
	if (strstr(buff, "USR") != NULL) {
		sscanf(strstr(buff, "USR"), "USR %d", &trId);
		snprintf(sendBuff, 512, "USR %d OK %s %s\r\n", trId, email, friendlyName);
		send(csocket, sendBuff, strlen(sendBuff), 0);
	}
	else if (strstr(buff, "CAL") != NULL) {
		char calledEmail[128];
		sscanf(strstr(buff, "CAL"), "CAL %d %s", &trId, calledEmail);
		snprintf(sendBuff, 512, "CAL %d RINGING 0\r\n", trId);
		send(csocket, sendBuff, strlen(sendBuff), 0);
		sscanf(calledEmail, "%lld@hotmail.com", id);
		data->csocket = csocket;
		data->friendlyName = calledEmail;
		data->id = *id;
		setSwitchboardSocket(*data);
		snprintf(sendBuff, 512, "JOI %s %s\r\n", calledEmail, calledEmail);
		send(csocket, sendBuff, strlen(sendBuff), 0);
	}
	else if (strstr(buff, "OUT") != NULL) {
		closesocket(csocket);
		free(buff);
		deleteSwitchboardSocket(*data);
		printf("closing switchboard\n");
		ExitThread(0);
	}
	else if (strstr(buff, "MSG") != NULL) {
		char msgType[2];
		sscanf(strstr(buff, "MSG"), "MSG %d %s", &trId, msgType);
		if (strstr(buff, "TypingUser") != NULL) {
			setTyping(*id);
			return;
		}
		if (strncmp(msgType, "N", 1) == 0) {
			telebot_error_e err;
			err = sendMessageToTelegram(strstr(buff, "\r\n\r\n") + 4, *id);
			if (err != TELEBOT_ERROR_NONE) {
				snprintf(sendBuff, 512, "NAK %d\r\n", trId);
				send(csocket, sendBuff, strlen(sendBuff), 0);
			}
			else {
				snprintf(sendBuff, 512, "ACK %d\r\n", trId);
				send(csocket, sendBuff, strlen(sendBuff), 0);
			}
		}
	}
	else if (strstr(buff, "ANS") != NULL) {
		char email[128];
		char firstName[128];
		int sessionId;
		sscanf(strstr(buff, "ANS"), "ANS %d %s %lld)%s %d", &trId, email, id, firstName, &sessionId);
		data->csocket = csocket;
		data->friendlyName = firstName;
		data->id = *id;
		setSwitchboardSocket(*data);
		snprintf(sendBuff, 512, "IRO %d 1 2 %lld@hotmail.com %s\r\n", trId, *id, firstName);
		send(csocket, sendBuff, strlen(sendBuff), 0);
		snprintf(sendBuff, 512, "IRO %d 2 2 %s %s\r\n", trId, email, friendlyName);
		send(csocket, sendBuff, strlen(sendBuff), 0);
		snprintf(sendBuff, 512, "ANS %d OK\r\n", trId);
		send(csocket, sendBuff, strlen(sendBuff), 0);
		snprintf(sendBuff, 512, "JOI %lld@hotmail.com %s\r\n", *id, firstName);
		send(csocket, sendBuff, strlen(sendBuff), 0);
	}
}