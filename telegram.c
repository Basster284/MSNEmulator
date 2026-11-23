#include "telegram.h"

SOCKET csocket;
SwitchBoardData switchboards[50] = { 0 };
telebot_handler_t handler = { 0 };
sqlite3* sql;
int sessId = 0;
bool nsLive = false;
unsigned short port = 1864;

void setSql(sqlite3* nsql) {
	sql = nsql;
}

void setNSLive(bool live) {
	nsLive = live;
}

void startBot() {
	char path[MAX_PATH];
	char token[50];
	GetModuleFileNameA(NULL, path, MAX_PATH);
	PathRemoveFileSpecA(path);
	strcat_s(path, MAX_PATH, "\\config.ini");
	GetPrivateProfileStringA("Telegram", "token", "TOKEN", token, sizeof(token), path);
	if (strcmp(token, "TOKEN") == 0) {
		printf("no token provided\n");
		exit(1);
	}
	telebot_create(&handler, token);
	telebot_user_t user = { 0 };
	if (telebot_get_me(handler, &user) != TELEBOT_ERROR_NONE) {
		printf("getme failed\n");
		exit(1);
	}
	printf("bot started\n");
	telebot_put_me(&user);
}

void setNotificationSocket(SOCKET nsocket) {
	csocket = nsocket;
}

void setSwitchboardSocket(SwitchBoardData data) {
	for (int i = 0; i < 50; i++) {
		if (switchboards[i].id == 0) {
			switchboards[i] = data;
			return;
		}
	}
}

u_short addPortAndGet() {
	port++;
	return port;
}

void deleteSwitchboardSocket(SwitchBoardData data) {
	for (int i = 0; i < 50; i++) {
		if (switchboards[i].id == data.id) {
			switchboards[i].id = 0;
			switchboards[i].csocket = 0;
			switchboards[i].friendlyName = 0;
			return;
		}
	}
}

telebot_user_t getUserById(int id, long long chatId) {
	telebot_user_t user = { 0 };
	telebot_get_chat_member(handler, id, chatId, &user);
	return user;
}

DWORD WINAPI update(LPVOID lpParam) {
	int index, count, offset = -1;
	telebot_error_e ret;
	telebot_message_t message;
	telebot_update_type_e update_types[] = { TELEBOT_UPDATE_TYPE_MESSAGE };
	while (1) {
		telebot_update_t* updates;
		ret = telebot_get_updates(handler, offset, 20, 0, update_types, 0, &updates, &count);
		if (ret != TELEBOT_ERROR_NONE)
			continue;
		for (index = 0; index < count; index++)
		{
			message = updates[index].message;
			if (message.text) {
				bool sended = false;
				while (nsLive == false) {
					Sleep(50);
				}
				// group
				if (strcmp(message.chat->type, "group") == 0 || strcmp(message.chat->type, "supergroup") == 0) {
					for (int i = 0; i < 50; i++) {
						if (switchboards[i].id == message.chat->id) {
							sendMessageToMSN(message.text, message.message_id, message.from->first_name, message.from->id, true, message.chat->id);
							sended = true;
						}
					}
					if (sended == false) {
						for (long long i = 0; i < strlen(message.chat->title); i++) {
							if (message.chat->title[i] == 0x20) {
								message.chat->title[i] = '_';
							}
						}
						u_short nport = addPortAndGet();
						CreateThread(NULL, 0, startSwitchboard, (LPVOID)(DWORD_PTR)nport, 0, NULL);
						Sleep(200);
						sendRing(message.chat->id, message.chat->title, nport);
						Sleep(200);
						sendMessageToMSN(message.text, message.message_id, message.from->first_name, message.from->id, true, message.chat->id);
						sqlite3_stmt* stmt;
						sqlite3_prepare_v2(sql, "INSERT INTO msn VALUES (?, ?) ON CONFLICT DO NOTHING", -1, &stmt, 0);
						sqlite3_bind_int64(stmt, 1, message.chat->id);
						sqlite3_bind_text(stmt, 2, message.chat->title, -1, SQLITE_TRANSIENT);
						sqlite3_step(stmt);
						sqlite3_finalize(stmt);
					}
				}
				// p2p
				else {
					sqlite3_stmt* stmt;
					sqlite3_prepare_v2(sql, "INSERT INTO msn VALUES (?, ?) ON CONFLICT DO NOTHING", -1, &stmt, 0);
					sqlite3_bind_int64(stmt, 1, message.from->id);
					sqlite3_bind_text(stmt, 2, message.from->first_name, -1, SQLITE_TRANSIENT);
					sqlite3_step(stmt);
					sqlite3_finalize(stmt);
					for (int i = 0; i < 50; i++) {
						if (switchboards[i].id == message.from->id) {
							sendMessageToMSN(message.text, message.message_id, message.from->first_name, message.from->id, false, 0);
							sended = true;
						}
					}
					if (sended == false) {
						u_short nport = addPortAndGet();
						CreateThread(NULL, 0, startSwitchboard, (LPVOID)(DWORD_PTR)nport, 0, NULL);
						Sleep(200);
						sendRing(message.from->id, message.from->first_name, nport);
						Sleep(200);
						sendMessageToMSN(message.text, message.message_id, message.from->first_name, message.from->id, false, 0);
					}
				}
			}
			offset = updates[index].update_id + 1;
		}
		telebot_put_updates(updates, count);
	}
}

telebot_error_e sendMessageToTelegram(const char* text, long long id) {
	char path[MAX_PATH];
	char friendlyName[50];
	telebot_error_e err;
	GetModuleFileNameA(NULL, path, MAX_PATH);
	PathRemoveFileSpecA(path);
	strcat_s(path, MAX_PATH, "\\config.ini");
	GetPrivateProfileStringA("MSN", "friendlyName", "Edit%20Name%20In%20Config.ini", friendlyName, sizeof(friendlyName), path);
	char sendBuff[4096];
	if (strcmp("/dice", text) == 0) {
		err = telebot_send_dice(handler, id, false, 0, NULL);
		if (err != TELEBOT_ERROR_NONE) {
			printf("failed to send\n");
			return err;
		}
		return TELEBOT_ERROR_NONE;
	}
	else if (strstr(text, "/reply") != NULL) {
		long long messageId;
		char reply[4096];
		sscanf(text, "/reply %lld %s", &messageId, reply);
		snprintf(sendBuff, 4096, "<b>%s:</b> %s", friendlyName, reply);
		err = telebot_send_message(handler, id, sendBuff, "HTML", false, false, messageId, NULL);
		if (err != TELEBOT_ERROR_NONE) {
			printf("failed to send\n");
			return err;
		}
		return TELEBOT_ERROR_NONE;
	}
	snprintf(sendBuff, 4096, "<b>%s:</b> %s", friendlyName, text);
	err = telebot_send_message(handler, id, sendBuff, "HTML", true, false, 0, NULL);
	if (err != TELEBOT_ERROR_NONE) {
		printf("failed to send\n");
		return err;
	}
	return TELEBOT_ERROR_NONE;
}

void sendRing(long long id, const char* name, u_short nport) {
	char sendBuff[512];
	snprintf(sendBuff, 512, "RNG 0 127.0.0.1:%d CKI %lld)%s %lld@hotmail.com %s\r\n", nport, id, name, id, name);
	int iResult = send(csocket, sendBuff, strlen(sendBuff), 0);
}

void setTyping(long long chatId) {
	telebot_send_chat_action(handler, chatId, "typing");
}

void closeBot() {
	telebot_destroy(handler);
}

void sendMessageToMSN(const char* text, long long messageId, const char* firstName, long long id, bool group, long long groupId) {
	char sendBuff[5000];
	char message[5000];
	snprintf(message, 5000, "MIME-Version: 1.0\r\nContent-Type: text/plain; charset=UTF-8\r\n\r\n%s (id: %lld)", text, messageId);
	snprintf(sendBuff, 5000, "MSG %lld@hotmail.com %s %d\r\n%s\r\n", id, firstName, strlen(message), message);
	if (group == true) {
		for (int i = 0; i < 50; i++) {
			if (switchboards[i].id == groupId) {
				send(switchboards[i].csocket, sendBuff, strlen(sendBuff), 0);
				return;
			}
		}
	}
	for (int i = 0; i < 50; i++) {
		if (switchboards[i].id == id) {
			send(switchboards[i].csocket, sendBuff, strlen(sendBuff), 0);
			return;
		}
	}
}