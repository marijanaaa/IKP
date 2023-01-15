#pragma comment(lib, "Ws2_32.lib")
#define _WINSOCK_DEPRECATED_NO_WARNINGS 1
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <iostream>
#include "../Common/NetworkFunctions.cpp"
#define DEFAULT_PORT 27016
#define SERVER_ADDRESS "127.0.0.1"
#define BUFFER_LEN 256

bool InitializeWindowsSockets();

typedef enum {
	CLIENT_READY = 0,
	WAITING,
	PLAYING,
	HOST,
}ClientStates;

int __cdecl main(int argc, char** argv)
{
	// socket used to communicate with server
	SOCKET connectSocket = INVALID_SOCKET;
	// message to send

	if (InitializeWindowsSockets() == false)
	{
		// we won't log anything since it will be logged
		// by InitializeWindowsSockets() function
		return 1;
	}

	// create a socket
	connectSocket = socket(AF_INET,
		SOCK_STREAM,
		IPPROTO_TCP);

	if (connectSocket == INVALID_SOCKET)
	{
		printf("socket failed with error: %ld\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}
	int choice;
	while (true) {
		printf("Should you play using binary : (1 : yes, 0 : no)");
		scanf_s(" %d", &choice);
		if (choice == 1 || choice == 0) {
			break;
		}
	}

	getchar();
	// create and initialize address structure
	sockaddr_in serverAddress;
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
	serverAddress.sin_port = htons(DEFAULT_PORT);
	// connect to server specified in serverAddress and socket connectSocket
	if (connect(connectSocket, (SOCKADDR*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR)
	{
		printf("Unable to connect to server.\n");
		closesocket(connectSocket);
		WSACleanup();
	}

	DataTransfer data;
	char dataBuffer[BUFFER_LEN];
	ClientStates state = CLIENT_READY;
	int number = 0;
	int low = 0;
	int high = 0;
	int binaryLow = 0;
	int binaryHigh = 0;
	int lastGuess;
	int result;
	while (true) {
		switch (state) {
		case CLIENT_READY:
			printf("Send 'ready' when ready for game to start\n");
			gets_s(dataBuffer, BUFFER_LEN);

			if (strcmp(dataBuffer, "ready") != 0) {
				continue;
			}

			data.Type = DataType::READY;
			result = SendBuffer(connectSocket, &data, BUFFER_LEN);
			if (result == -1) {
				closesocket(connectSocket);
				WSACleanup();

				return 0;
			}
			
			if (result == 0) {
				closesocket(connectSocket);
				WSACleanup();

				return 0;
			}

			printf("Waiting for all players to get ready or the current game to finish\n");
			state = WAITING;
			break;
		case WAITING:
			result = RecieveBuffer(connectSocket, &data, BUFFER_LEN);
			if (result == -1) {
				closesocket(connectSocket);
				WSACleanup();

				return 0;
			}

			if (result == 0) {
				continue;
			}

			if (data.Type == READY_COUNT) {
				printf("%s\n", data.Message);
			}
			else if (data.Type == CHOOSE_NUMBER) {
				while (true) {
					printf("Enter bottom interval : ");
					scanf_s(" %d", &low);
					printf("Enter high interval : ");
					scanf_s(" %d", &high);
					printf("Enter number : ");
					scanf_s("%d", &number);
					getchar();

					if (low > high) {
						printf("Bottom interval should be lower than high interval\n");
						continue;
					}
					else if (number < low || number > high) {
						printf("Number should be in the interval.\n");
						continue;
					}
					else {
						break;
					}
				}

				data.Type = CHOOSE_NUMBER;
				data.low = low;
				data.high = high;
				int rec = SendBuffer(connectSocket, &data, BUFFER_LEN);
				if (rec == -1) {
					closesocket(connectSocket);
					WSACleanup();

					return 0;
				}

				if (rec == 0) {
					closesocket(connectSocket);
					WSACleanup();

					return 0;
				}

				state = HOST;
			}
			else if (data.Type == GAME_IN_PROGRESS) {
				low = data.low;
				high = data.high;
				binaryLow = low;
				binaryHigh = high;
				state = PLAYING;
			}

			break;
		case PLAYING:

			if (choice == 0) {
				printf("Guess number in interval (%d-%d):", low, high);

				scanf_s(" %d", &data.guess);
				getchar();
			}
			else {
				data.guess = (binaryHigh + binaryLow) / 2;
				lastGuess = data.guess;
				printf("Guessing automatically using binary, next number : %d\n", data.guess);
			}


			result = SendBuffer(connectSocket, &data, BUFFER_LEN);
			if (result == -1) {
				closesocket(connectSocket);
				WSACleanup();

				return 0;
			}
			
			if (result == 0) {
				closesocket(connectSocket);
				WSACleanup();

				return 0;
			}
			int result;
			while (true) {
				result = RecieveBuffer(connectSocket, &data, BUFFER_LEN);
				if (result == -1) {
					closesocket(connectSocket);
					WSACleanup();

					return 0;
				}

				if (result != 0) {
					if (data.Type == READY_COUNT) {
						printf("%s\n", data.Message);
						continue;
					}

					break;
				}
			}

			if (data.Type == GAME_IN_PROGRESS) {
				if (data.guess == 1) {
					printf("THE NUMBER IS GREATER THAN THE ONE YOU GUESSED!\n");

					if (choice == 1) {
						binaryLow = lastGuess + 1;
					}
				}
				else if (data.guess == -1) {
					printf("THE NUMBER IS LOWER THAN THE ONE YOU GUESSED!\n");

					if (choice == 1) {
						binaryHigh = lastGuess - 1;
					}
				}
			}
			else if (data.Type == FINISHED) {
				if (data.guess == 0) {
					printf("YOU GUESSED CORRECTLY, YOU WON!\n");
				}
				else {
					printf("YOU LOST, SOMEONE GUESSED BEFORE YOU!\n");
				}

				state = CLIENT_READY;
			}
		case HOST:
			result = RecieveBuffer(connectSocket, &data, BUFFER_LEN);
			if (result == -1) {
				closesocket(connectSocket);
				WSACleanup();

				return 0;
			}

			if (result == 0) {
				break;
			}

			if (data.Type == GAME_IN_PROGRESS) {
				while (true) {
					if (data.guess > number) {
						data.guess = -1;
						printf("Number is lower!\n");
						break;
					}
					else if (data.guess < number) {
						data.guess = 1;
						printf("Number is higher!\n");
						break;
					}
					else if (data.guess == number) {
						data.guess = 0;
						printf("Number is correct!\n");
						break;
					}
					else {
						printf("Wrong choice, try again.\n");
					}
				}

				result = SendBuffer(connectSocket, &data, BUFFER_LEN);
				if (result == -1) {
					closesocket(connectSocket);
					WSACleanup();

					return 0;
				}

				if (result == 0) {
					closesocket(connectSocket);
					return 0;
				}
			}
			else if (data.Type == FINISHED) {
				printf("SOMEONE GUESSED, THE GAME IS OVER, STARTING A NEW ONE!\n");
				state = CLIENT_READY;
			}
		}
	}

	return 0;
}

bool InitializeWindowsSockets()
{
	WSADATA wsaData;
	// Initialize windows sockets library for this process
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		printf("WSAStartup failed with error: %d\n", WSAGetLastError());
		return false;
	}
	return true;
}
