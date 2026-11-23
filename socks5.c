#include "socks5.h"

/*THIS THING DOES NOT WORK AT ALL
THESE STUPID SWITCHBOARDS AINT DOING ANYTHING
I DONT KNOW WHAT TO DO*/

void processSocket(SOCKET csocket, char* buff, int buffLen);

void startProxy() {
	struct addrinfo* result = NULL;
	struct addrinfo hints = { 0 };

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	int iResult = getaddrinfo(NULL, "1080", &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed: %d\n", iResult);
		WSACleanup();
		exit(1);
	}

	SOCKET lsocket = INVALID_SOCKET;
	lsocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (lsocket == INVALID_SOCKET) {
		printf("socket error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		exit(1);
	}

	iResult = bind(lsocket, result->ai_addr, (int)result->ai_addrlen);
	freeaddrinfo(result);
	if (iResult == SOCKET_ERROR) {
		printf("bind error: %ld\n", WSAGetLastError());
		closesocket(lsocket);
		WSACleanup();
		exit(1);
	}

	if (listen(lsocket, SOMAXCONN) == SOCKET_ERROR) {
		printf("listen failed: %ld", WSAGetLastError());
		closesocket(lsocket);
		WSACleanup();
		exit(1);
	}

	char* buff = malloc(sizeof(char) * 8192);
	if (buff == NULL) {
		printf("malloc failed\n");
		closesocket(lsocket);
		WSACleanup();
		exit(1);
	}

	puts("socks5 started");
	puts("ip: 127.0.0.1");
	puts("port: 1080");

	SOCKET csocket = INVALID_SOCKET;
	while (1) {
		puts("waiting");
		csocket = accept(lsocket, NULL, NULL);
		if (csocket == INVALID_SOCKET) {
			printf("accept error: %ld\n", WSAGetLastError());
			closesocket(csocket);
			WSACleanup();
			return 1;
		}

		while (1) {
			memset(buff, 0, 8192);
			iResult = recv(csocket, buff, 8192, 0);
			if (iResult > 0) {
				processSocket(csocket, buff, iResult);
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
		puts("connection closed");
	}
	closesocket(csocket);
	WSACleanup();
	free(buff);
	return 0;
}

void processSocket(SOCKET csocket, char* buff, int buffLen) {
	if (buff[0] == 0x05) {
		if (buff[1] == 0x02) {
			char sendBuff[2] = { 0x05, 0x00 };
			send(csocket, sendBuff, 2, 0);
		}
		else if (buff[1] == 0x01) {
			buff[1] = 0x00;
			for (int i = 0; i < buffLen; i++) {
				printf("%d ", buff[i]);
			}
			send(csocket, buff, buffLen, 0);
		}
	}
	else {
		SOCKET msnSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		struct sockaddr_in serv;
		serv.sin_family = AF_INET;
		serv.sin_addr.s_addr = inet_addr("127.0.0.1");
		serv.sin_port = htons(1863);

		if (connect(msnSocket, (struct sockaddr*)&serv, sizeof(serv)) == SOCKET_ERROR) {
			puts("failed to connect to local server (lol)");
			closesocket(csocket);
			WSACleanup();
			exit(1);
		}

		char sendBuff[8192];

		send(msnSocket, buff, buffLen, 0);

		while (1) {
			FD_SET fd;
			FD_ZERO(&fd);
			FD_SET(csocket, &fd);
			FD_SET(msnSocket, &fd);

			select(0, &fd, NULL, NULL, NULL);
			if (FD_ISSET(csocket, &fd)) {
				int iResult = recv(csocket, sendBuff, 8192, 0);
				if (iResult > 0)
				send(msnSocket, sendBuff, iResult, 0);
				else if (iResult == 0) {
					puts("closing");
					closesocket(msnSocket);
					closesocket(csocket);
					break;
				}
				else {
					puts("disconnected");
					closesocket(msnSocket);
					closesocket(csocket);
					break;
				}
			}
			if (FD_ISSET(msnSocket, &fd)) {
				int iResult = recv(msnSocket, sendBuff, 8192, 0);
				if (iResult > 0)
					send(csocket, sendBuff, iResult, 0);
				else if (iResult == 0) {
					puts("closing");
					closesocket(msnSocket);
					closesocket(csocket);
					break;
				}
				else {
					puts("disconnected");
					closesocket(msnSocket);
					closesocket(csocket);
					break;
				}
			}
		}
	}
}