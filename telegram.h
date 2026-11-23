#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include "telebot/telebot.h"
#include <stdio.h>
#include <WinSock2.h>
#include <sqlite3.h>
#include <shlwapi.h>
#include "switchboard.h"

#pragma comment(lib, "Shlwapi.lib")

void startBot();
DWORD WINAPI update(LPVOID lpParam);
void setSql(sqlite3* nsql);
void setNotificationSocket(SOCKET csocket);
telebot_error_e sendMessageToTelegram(const char* text, long long id);
void sendMessageToMSN(const char* text, long long messageId, const char* firstName, long long id, bool group, long long groupId);
void setSwitchboardSocket(SwitchBoardData data);
void setNSLive(bool live);
void closeBot();
void setTyping(long long id);
void sendRing(long long id, const char* text, u_short nport);
void deleteSwitchboardSocket(SwitchBoardData data);
u_short addPortAndGet();