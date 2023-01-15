#include "NetworkFunctions.h"
#include <stdio.h>
#include "FunctionCodes.h"

typedef enum {
	READY = 0,
	READY_COUNT,
	GAME_IN_PROGRESS,
	CHOOSE_NUMBER,
	FINISHED,
} DataType;


struct DataTransfer {
	DataType Type;
	char Message[100];
	int low;
	int high;
	int guess;
};

bool AcceptSocket(SOCKET s, SOCKET *acceptedSocket) {
	int selectResult = CustomSelect(s, READ);
	if (selectResult == SELECT_READ) {
		sockaddr_in clientAddr;
		int clientAddrSize = sizeof(struct sockaddr_in);
		*acceptedSocket = accept(s, (struct sockaddr*)&clientAddr, &clientAddrSize);
		return true;
	}

	return false;
}

int SendBuffer(SOCKET s, const DataTransfer* data, int length) {
	int iResult = 0;
	int sentLength = 0;

	int selectResult;

	char* buff = (char*)data;
	while (sentLength < length) {

		selectResult = CustomSelect(s, WRITE);
		iResult = send(s, buff + sentLength, length - sentLength, 0);

		sentLength += iResult;

		if (iResult == SOCKET_ERROR)
			return -1;
	}

	return sentLength;
}

int RecieveBuffer(SOCKET s, DataTransfer *data, int length) {
	int iResult = 0;
	int recievedLength = 0;

	char* buffer = (char*)data;
	int selectResult;
	while (recievedLength < length) {

		selectResult = CustomSelect(s, READ);


		if (selectResult == SELECT_READ) {
			// Receive data until the client shuts down the connection
			iResult = recv(s, buffer + recievedLength, length - recievedLength, 0);

			if (iResult > 0)
			{
				recievedLength += iResult;
			}

			if (iResult == 0) {
				return -1;
			}
		}
		else if (selectResult == SELECT_TIMEOUT) {
			break;
		}
	}

	return recievedLength;
}

int CustomSelect(SOCKET s, OperationCodes op) {
	timeval timeVal;
	timeVal.tv_sec = 0;
	timeVal.tv_usec = 10;

	fd_set readfds;
	fd_set writefds;

	while (true) {

		FD_ZERO(&readfds);
		FD_ZERO(&writefds);

		if (op == READ) {
			FD_SET(s, &readfds);
		}
		else if (op == WRITE) {
			FD_SET(s, &writefds);
		}
		else {
			FD_SET(s, &readfds);
			FD_SET(s, &writefds);
		}


		// wait for events on set
		int selectResult = select(0, &readfds, &writefds, NULL, &timeVal);

		if (selectResult == SOCKET_ERROR)
		{
			printf("Select failed with error: %d\n", WSAGetLastError());
			return SELECT_ERROR;
		}
		else if (selectResult == 0) // timeout expired
		{
			return SELECT_TIMEOUT; 
		}
		else {

			if ((FD_ISSET(s, &readfds)) && FD_ISSET(s, &writefds))
				return SELECT_READWRITE;
			else if ((FD_ISSET(s, &readfds)))
				return SELECT_READ;
			else if ((FD_ISSET(s, &writefds)))
				return SELECT_WRITE;

		}
	}
}
