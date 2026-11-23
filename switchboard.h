#pragma once

#undef UNICODE

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <sqlite3.h>

#pragma comment (lib, "Ws2_32.lib")

typedef struct {
	long long id;
	char* friendlyName;
	SOCKET csocket;
} SwitchBoardData;

typedef struct {
	char* id;
	char* friendlyName;
	SOCKET csocket;
} MatrixSwitchBoardData;

DWORD WINAPI startSwitchboard(LPVOID lpParam);
void processBoard(SOCKET csocket, const char* buff, long long* id, SwitchBoardData* data);
void processMatrixBoard(SOCKET csocket, const char* buff, MatrixSwitchBoardData* data, const char* email, const char* friendlyName);