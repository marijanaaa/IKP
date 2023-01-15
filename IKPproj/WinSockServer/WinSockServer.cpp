// WinSockServer.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#pragma comment(lib, "Ws2_32.lib")
#define _WINSOCK_DEPRECATED_NO_WARNINGS 1
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <iostream>
#include "../ProducerConsumer/Source.cpp"
#include "../Common/HashMap.h"
#include "../Common/NetworkFunctions.cpp"
#define SAFE_DELETE_HANDLE(a)  if(a){CloseHandle(a);}

typedef enum {
	LOBBY = 0,
	STARTED,
} GameState;


bool InitializeWindowsSockets();

#define INIT_BUFLEN 4
#define BUFFER_LEN 256
#define DEFAULT_PORT "27016"
#define THREAD_COUNT 3
#define USERS 100000
struct ClientInfo
{
	int id;
	SOCKET acceptedSocket; 
	int guess;
	bool isReady;
};

int currentId = 1500;
int serverWorking = 1;

struct AcceptClientsInput {
	ProducerConsumer<ClientInfo>* Lobby;
	HANDLE* FinishSignal;
	SOCKET ListenSocket;
};

struct AddClientsInput {
	ProducerConsumer<ClientInfo>* Lobby;
	ProducerConsumer<int>* ClientsQueue;
	CRITICAL_SECTION* GameSetup;
	HashMap<ClientInfo>* Clients;
	HANDLE* FinishSignal;
	GameState* IsGameStarted;
};

struct GameHandlerInput {
	ProducerConsumer<int>* ClientsQueue;
	HashMap<ClientInfo>* Clients;
	ProducerConsumer<int>* InGameQueue;
	HashMap<ClientInfo>* InGame;

	CRITICAL_SECTION* GameSetup;
	GameState* IsGameStarted;
	HANDLE* FinishSignal;
	int *ReadyCount;
	ClientInfo *Host;
	int *Low;
	int *High;
	int *Guess;
};
struct FeedClientsToProcessInput {
	ProducerConsumer<int>* ClientsQueue;
	HashMap<ClientInfo>* Clients;

	HashMap<ClientInfo>* InGame;

	CRITICAL_SECTION* GameSetup;
	HANDLE* FinishSignal;
	GameState* IsGameStarted;
};

struct ShutDown
{
	HANDLE* FinishSignal;
	int* serverWorking;
};

DWORD WINAPI AcceptClients(LPVOID param);
DWORD WINAPI AddClients(LPVOID param);
DWORD WINAPI GameHandler(LPVOID param);
DWORD WINAPI FeedClientsToProcess(LPVOID param);
DWORD WINAPI ReadyCounter(LPVOID param);
int ClearClientsMap(HashMap<ClientInfo>* clients);
DWORD WINAPI ProcessAdminInput(LPVOID param);
void free_fields(char** options, int count);
int main()
{
	srand(time(NULL));
	// Socket used for listening for new clients 
	SOCKET listenSocket = INVALID_SOCKET;
	// Socket used for communication with client
	SOCKET acceptedSocket = INVALID_SOCKET;
	// variable used to store function return value
	int iResult;
	int numberOfThreads = 0;
	// Buffer used for storing incoming data 
	char bufferSize[INIT_BUFLEN];
	char* recvBuffer;
	int initBufferSize;

	// Sockets used for communication with client
	SOCKET clientSocket;
	short lastIndex = 0;

	if (InitializeWindowsSockets() == false)
	{
		// we won't log anything since it will be logged
		// by InitializeWindowsSockets() function
		return 1;
	}

	// Prepare address information structures
	addrinfo* resultingAddress = NULL;
	addrinfo hints;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;       // IPv4 address
	hints.ai_socktype = SOCK_STREAM; // Provide reliable data streaming
	hints.ai_protocol = IPPROTO_TCP; // Use TCP protocol
	hints.ai_flags = AI_PASSIVE;     // 

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &resultingAddress);
	if (iResult != 0)
	{
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	// Create a SOCKET for connecting to server
	listenSocket = socket(AF_INET,      // IPv4 address famly
		SOCK_STREAM,  // stream socket
		IPPROTO_TCP); // TCP

	if (listenSocket == INVALID_SOCKET)
	{
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(resultingAddress);
		WSACleanup();
		return 1;
	}

	// Set socket to be non-blocking
	unsigned long int mode = 1;
	iResult = ioctlsocket(listenSocket, FIONBIO, &mode);

	if (iResult < 0) {
		printf("non-blocking failed with error: %d\n", WSAGetLastError());
		closesocket(listenSocket);
		WSACleanup();
		return 1;
	}

	// Setup the TCP listening socket - bind port number and local address 
	// to socket
	iResult = bind(listenSocket, resultingAddress->ai_addr, (int)resultingAddress->ai_addrlen);
	if (iResult == SOCKET_ERROR)
	{
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(resultingAddress);
		closesocket(listenSocket);
		WSACleanup();
		return 1;
	}

	// Since we don't need resultingAddress any more, free it
	freeaddrinfo(resultingAddress);

	// Set listenSocket in listening mode
	iResult = listen(listenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR)
	{
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(listenSocket);
		WSACleanup();
		return 1;
	}

	HANDLE FinishSignal = CreateSemaphore(0, 0, 9, NULL);
	HANDLE processors[9] = { NULL };

	ProducerConsumer<ClientInfo> lobby(&FinishSignal, 50);
	//HANDLE AcceptClientsController = NULL;
	DWORD AcceptClientsControllerID;

	AcceptClientsInput acceptClientsInput = AcceptClientsInput();
	acceptClientsInput.ListenSocket = listenSocket;
	acceptClientsInput.Lobby = &lobby;
	acceptClientsInput.FinishSignal = &FinishSignal;

	processors[0] = CreateThread(NULL, 0, &AcceptClients, (LPVOID)&acceptClientsInput, 0, &AcceptClientsControllerID);
	//processors[0] = AcceptClientsController;
	numberOfThreads++;

	CRITICAL_SECTION GameSetup;
	GameState isGameStarted = LOBBY;
	HashMap<ClientInfo> clients;
	HashMap<ClientInfo> inGame;

	InitializeCriticalSection(&GameSetup);
	
	ProducerConsumer<int> clientsQueue(&FinishSignal, 50);

	AddClientsInput addClientInput = AddClientsInput();
	addClientInput.GameSetup = &GameSetup;
	addClientInput.Lobby = &lobby;
	addClientInput.IsGameStarted = &isGameStarted;
	addClientInput.Clients = &clients;
	addClientInput.FinishSignal = &FinishSignal;

	//HANDLE AddClientsController[THREAD_COUNT];
	DWORD AddClientsControllerID[THREAD_COUNT];
	for (int i = 0; i < THREAD_COUNT; i++) {
		processors[i + 1] = CreateThread(NULL, 0, &AddClients, (LPVOID)&addClientInput, 0, &AddClientsControllerID[i]);
		numberOfThreads++;
	}
	
	//HANDLE FeedClientsToProcessController = NULL;
	DWORD FeedClientsToProcessControllerID;
	
	FeedClientsToProcessInput feedClientsToProcessInput = FeedClientsToProcessInput();
	feedClientsToProcessInput.GameSetup = &GameSetup;
	feedClientsToProcessInput.IsGameStarted = &isGameStarted;
	feedClientsToProcessInput.Clients = &clients;
	feedClientsToProcessInput.InGame = &inGame;
	feedClientsToProcessInput.FinishSignal = &FinishSignal;
	feedClientsToProcessInput.ClientsQueue = &clientsQueue;

	processors[4] = CreateThread(NULL, 0, &FeedClientsToProcess, (LPVOID)&feedClientsToProcessInput, 0, &FeedClientsToProcessControllerID);
	//processors[4] = FeedClientsToProcessController;
	numberOfThreads++;

	//HANDLE gameHandlerController[THREAD_COUNT];
	DWORD gameHandlerControllerID[THREAD_COUNT];

	int readyCount = 0;
	int low;
	int high;
	int guess;
	ClientInfo host;
	GameHandlerInput gameHandlerInput = GameHandlerInput();
	gameHandlerInput.GameSetup = &GameSetup;
	gameHandlerInput.IsGameStarted = &isGameStarted;
	gameHandlerInput.Clients = &clients;
	gameHandlerInput.InGame = &inGame;
	gameHandlerInput.FinishSignal = &FinishSignal;
	gameHandlerInput.ClientsQueue = &clientsQueue;
	gameHandlerInput.ReadyCount = &readyCount;
	gameHandlerInput.Low = &low;
	gameHandlerInput.High = &high;
	gameHandlerInput.Host = &host;
	gameHandlerInput.Guess = &guess;

	for (int i = 0; i < THREAD_COUNT; i++) {
		processors[i + 5] = CreateThread(NULL, 0, &GameHandler, (LPVOID)&gameHandlerInput, 0, &gameHandlerControllerID[i]);
		//processors[i + 5] = gameHandlerController[i];
		numberOfThreads++;
	}

	//HANDLE readyCounterController = NULL;
	DWORD readyCounterControllerID;

	GameHandlerInput readyCounterInput = GameHandlerInput();
	readyCounterInput.GameSetup = &GameSetup;
	readyCounterInput.IsGameStarted = &isGameStarted;
	readyCounterInput.Clients = &clients;
	readyCounterInput.FinishSignal = &FinishSignal;
	readyCounterInput.ClientsQueue = &clientsQueue;
	readyCounterInput.ReadyCount = &readyCount;
	readyCounterInput.Low = &low;
	readyCounterInput.High = &high;
	readyCounterInput.Host = &host;
	readyCounterInput.Guess = &guess;
	readyCounterInput.InGame = &inGame;

	processors[8] = CreateThread(NULL, 0, &ReadyCounter, (LPVOID)&readyCounterInput, 0, &readyCounterControllerID);
	//processors[8] = readyCounterController;
	numberOfThreads++;
	printf("Press Q or q to shutdown\n");
	getchar();
	ReleaseSemaphore(FinishSignal, 9, NULL);

	//ReleaseSemaphore(FinishSignal, numberOfThreads, NULL);
	printf("Server is not listening anymore!\n");
	for (int i = 0; i < numberOfThreads; i++) {
		int result = WaitForSingleObject(processors[i], INFINITE);
		printf("%d\n",result);
	}

	for (int i = 0; i < 9; i++)
		SAFE_DELETE_HANDLE(processors[i]);

	int socketsCount = 0;
	
	int keyCount = 0;
	char** users = clients.GetKeys(&keyCount);
	for (int i = 0; i < keyCount; i++) {
		ClientInfo clientInfo;
		clients.Get(users[i], &clientInfo);
		int iResult = shutdown(clientInfo.acceptedSocket, SD_BOTH);
		printf("aa %d\n", iResult);
		closesocket(clientInfo.acceptedSocket);
	}
	free_fields(users, keyCount);



	WSACleanup();
	closesocket(listenSocket);
	
	clientsQueue.Clear();
	lobby.Clear();
	
	DeleteCriticalSection(&GameSetup);
	SAFE_DELETE_HANDLE(FinishSignal);
	
	
	printf("\nPress any key to close");
	getchar();
	return 0;
}

DWORD WINAPI AcceptClients(LPVOID param) {
	AcceptClientsInput* input= (AcceptClientsInput*)param;
	
	while (WaitForSingleObject(*input->FinishSignal, 0) != WAIT_OBJECT_0) {
		//printf("ACCEPT_CLIENTS : %d\n", WaitForSingleObject(*input->FinishSignal, 0));
		sockaddr_in clientAddr;
		int clientAddrSize = sizeof(struct sockaddr_in);
		SOCKET acceptedSocket;
		bool isAccepted = AcceptSocket(input->ListenSocket, &acceptedSocket);
		if (!isAccepted) {
			continue;
		}

		printf("[AcceptClients]: Client accepted\n");
		ClientInfo info = ClientInfo();
		info.id = currentId++;
		info.acceptedSocket = acceptedSocket;

		// Set socket to be non-blocking
		unsigned long int mode = 1;
		int iResult = ioctlsocket(info.acceptedSocket, FIONBIO, &mode);

		if (iResult < 0) {
			printf("non-blocking failed with error: %d\n", WSAGetLastError());
			closesocket(info.acceptedSocket);
			WSACleanup();
			return 1;
		}
		while (true) {
			int isFinished = input->Lobby->Produce(info);
			if (isFinished == 0) {
				break;
			}
			else if (isFinished == -1) {
				printf("[Accept clients] ended thread...\n");
				return 0;
			}
		}
	}
	printf("[Accept clients] ended thread...\n");

    return 0;
}

DWORD WINAPI AddClients(LPVOID param) {
	AddClientsInput* input = (AddClientsInput*)param;
	ClientInfo clientInfo;
	while (true) {
		int result = input->Lobby->Consume(&clientInfo);
		if (result == -1) {
			// FINISH_SIGNAL
			printf("[AddClients] ended thread...\n");
			return 0;
		}
		else if (result == 1) {
			// TIMEOUT
			continue;
		}
		
		printf("[AddClients]: Client with ID accepted %d\n", clientInfo.id);
		char val[100];
		sprintf_s(val, "%d", clientInfo.id);
		
		EnterCriticalSection(input->GameSetup);
		input->Clients->Insert(val, clientInfo);
		LeaveCriticalSection(input->GameSetup);
	}
	printf("[Add clients] ended thread...\n");
	return 0;
}

DWORD WINAPI FeedClientsToProcess(LPVOID param) {
	FeedClientsToProcessInput* input = (FeedClientsToProcessInput*)param;
	
	while (WaitForSingleObject(*input->FinishSignal, 0) != WAIT_OBJECT_0) {
		EnterCriticalSection(input->GameSetup);
		int count = 0;
		char** copy;
		if (*input->IsGameStarted == LOBBY){
			copy = input->Clients->GetKeys(&count);
		}
		else {
			copy = input->InGame->GetKeys(&count);
		}

		ClientInfo clientInfo;
		for (int i = 0; i < count;i++) {
    		int result = input->ClientsQueue->Produce(atoi(copy[i]));
			if (result == 1) {
				LeaveCriticalSection(input->GameSetup);
				Sleep(10);
				EnterCriticalSection(input->GameSetup);
				continue;
			}
			else if (result == -1) {
				printf("[FeedClientsToProcess] ended thread...\n");
				free_fields(copy, count);
				LeaveCriticalSection(input->GameSetup);
				return 0;
			}
		}

		free_fields(copy, count);

		LeaveCriticalSection(input->GameSetup);
		Sleep(10);
	}
	printf("[FeedClientsToProcess] ended thread...\n");

	return 0;
}

void free_fields(char** options, int count)
{
	if (count == 0) {
		return;
	}

	for (int i = 0; i < count; i++)
		free(options[i]);
	free(options);
}


DWORD WINAPI GameHandler(LPVOID param) {
	GameHandlerInput* input = (GameHandlerInput*)param;

	int clientId;
	// Check whether to shutdown
	while (WaitForSingleObject(*input->FinishSignal, 0) != WAIT_OBJECT_0) {
		EnterCriticalSection(input->GameSetup);

		int result = input->ClientsQueue->Consume(&clientId);
		if (result == -1) {
			// FINISH_SIGNAL
			printf("[GameHandler] ended thread...\n");
			LeaveCriticalSection(input->GameSetup);
			return 0;
		}
		else if (result == 1) {
			// TIMEOUT
			LeaveCriticalSection(input->GameSetup);
			Sleep(10);
			continue;
		}
		
		ClientInfo clientInfo;

		char val[100];
		sprintf_s(val, "%d", clientId);

		bool isExist = input->Clients->Get(val, &clientInfo);
		LeaveCriticalSection(input->GameSetup);

		if (!isExist){
			continue;
		}

		DataTransfer data;
		int received = RecieveBuffer(clientInfo.acceptedSocket, &data, BUFFER_LEN);
		if (received == 0) {
			continue;
		}

		if (data.Type == READY) {
			printf("[GameHandler]: Client ready %d\n", clientInfo.id);
			clientInfo.isReady = true;
			EnterCriticalSection(input->GameSetup);


			input->Clients->Insert(val, clientInfo);
			LeaveCriticalSection(input->GameSetup);

			(*input->ReadyCount)++;
		}
		else if (data.Type == GAME_IN_PROGRESS) {
			int result = SendBuffer(input->Host->acceptedSocket, &data, BUFFER_LEN);
			if (result == 0) {
				continue;
			}

			while (true) {
				int result = RecieveBuffer(input->Host->acceptedSocket, &data, BUFFER_LEN);
				if (result != 0) {
					break;
				}
			}

			EnterCriticalSection(input->GameSetup);

			if (data.guess == 0) {
				printf("[GameHandler]: Saving response from client %d, response %s\n", clientInfo.id, "Correct!");
			}
			else if (data.guess == 1) {
				printf("[GameHandler]: Saving response from client %d, response %s\n", clientInfo.id, "Higher!");
			}
			else if (data.guess == -1) {
				printf("[GameHandler]: Saving response from client %d, response %s\n", clientInfo.id, "Lower!");
			}
			
			clientInfo.guess = data.guess;
			clientInfo.isReady = true;
			input->InGame->Insert(val, clientInfo);
			(*(input->ReadyCount))++;
			LeaveCriticalSection(input->GameSetup);
			Sleep(10);
		}
	}
	printf("[GameHandler] ended thread...\n");
	return 0;
}

DWORD WINAPI ReadyCounter(LPVOID param) {
	GameHandlerInput* input = (GameHandlerInput*)param;
	
	while (WaitForSingleObject(*input->FinishSignal, 0) != WAIT_OBJECT_0) {
		EnterCriticalSection(input->GameSetup);
		if (*input->IsGameStarted == LOBBY) {
			int keyCount = 0;
			char** users = input->Clients->GetKeys(&keyCount);

			if (keyCount >= 2 && *input->ReadyCount >= keyCount) {
				// IF GOOD NUMBERS OF PLAYERS, SEND ONE AT RANDOM TO CHOOSE A NUMBER
				DataTransfer data;
				data.Type = CHOOSE_NUMBER;
				int r = rand() % keyCount;

				ClientInfo clientInfo;
				bool isExist = input->Clients->Get(users[r], &clientInfo);
				if (!isExist){
					continue;
				}
				
				int result = SendBuffer(clientInfo.acceptedSocket, &data, BUFFER_LEN);
				if (result == 0) {
					continue;
				}

				printf("[ReadyCounter]: All players are ready. Waiting for client to respond with his interval and number\n");

				while (true) {
					if (WaitForSingleObject(*input->FinishSignal, 0) == WAIT_OBJECT_0) {
						free_fields(users, keyCount);

						LeaveCriticalSection(input->GameSetup);
						return 0;
					}

					int result = RecieveBuffer(clientInfo.acceptedSocket, &data, BUFFER_LEN);
					if (result != 0) {
						break;
					}
				}

				*input->High = data.high;
				*input->Low = data.low;
				(*input->Host).id = clientInfo.id;
				(*input->Host).acceptedSocket = clientInfo.acceptedSocket;
				*input->ReadyCount = 0;
				data.Type = GAME_IN_PROGRESS;
				sprintf_s(data.Message, "Guess number in interval [%d-%d]", data.low, data.high);
				*input->IsGameStarted = STARTED;
				
				int keyCountInGame = 0;
				char** usersInGame = input->InGame->GetKeys(&keyCountInGame);
				for (int i = 0; i < keyCountInGame; i++) {
					input->InGame->Delete(usersInGame[i]);
				}
				free_fields(usersInGame, keyCountInGame);

				input->ClientsQueue->Clear();
				for (int i = 0; i < keyCount; i++) {
					ClientInfo clientInfo;
					bool isExist = input->Clients->Get(users[i], &clientInfo);
					if (!isExist) {
						continue;
					}

					clientInfo.isReady = false;
					input->Clients->Insert(users[i], clientInfo);
					
					if (i == r) {
						continue;
					}

					input->InGame->Insert(users[i], clientInfo);

					SendBuffer(clientInfo.acceptedSocket, &data, BUFFER_LEN);
				}
			}
			else {
				// SEND USERS HOW MANY ARE READY
				DataTransfer data;
				data.Type = READY_COUNT;
				sprintf_s(data.Message, "Need more than 1. Waiting for players to get ready. [%d/%d]\n", *input->ReadyCount, keyCount);

				for (int i = 0; i < keyCount; i++) {
					ClientInfo clientInfo;
					bool isExist = input->Clients->Get(users[i], &clientInfo);
					if (!isExist) {
						continue;
					}

					if (!clientInfo.isReady) {
						continue;
					}

					SendBuffer(clientInfo.acceptedSocket, &data, BUFFER_LEN);
				}
			}
			free_fields(users, keyCount);
		}
		else if (*input->IsGameStarted == STARTED) {
			int keyCount = 0;
			char** users = input->InGame->GetKeys(&keyCount);

			if (*input->ReadyCount == keyCount) {
				bool isFinished = false;
				for (int i = 0; i < keyCount; i++) {
					ClientInfo clientInfo;
					bool isExist = input->InGame->Get(users[i], &clientInfo);
					if (!isExist) {
						continue;
					}

					if (clientInfo.guess == 0) {
						printf("[ReadyCounter]: Game is finished, starting a new one\n");
						isFinished = true;
					}
				}

				for (int i = 0; i < keyCount; i++) {
					ClientInfo clientInfo;
					bool isExist = input->InGame->Get(users[i], &clientInfo);
					if (!isExist) {
						continue;
					}

					DataTransfer data;
					data.Type = GAME_IN_PROGRESS;
					if (isFinished) {
						data.Type = FINISHED;
					}
					clientInfo.isReady = false;
					input->InGame->Insert(users[i], clientInfo);

					data.guess = clientInfo.guess;

					SendBuffer(clientInfo.acceptedSocket, &data, BUFFER_LEN);
				}

				if (isFinished) {
					input->ClientsQueue->Clear();
					*input->IsGameStarted = LOBBY;
					DataTransfer data;
					data.Type = FINISHED;
					SendBuffer(input->Host->acceptedSocket, &data,BUFFER_LEN);
				}
				*input->ReadyCount = 0;
			}
			else {
				// SEND USERS HOW MANY ARE READY
				DataTransfer data;
				data.Type = READY_COUNT;
				sprintf_s(data.Message, "Waiting for all guesses. [%d/%d]\n", *input->ReadyCount, keyCount);

				for (int i = 0; i < keyCount; i++) {
					ClientInfo clientInfo;
					bool isExist = input->InGame->Get(users[i], &clientInfo);
					if (!isExist) {
						continue;
					}

					if (!clientInfo.isReady) {
						continue;
					}
					SendBuffer(clientInfo.acceptedSocket, &data, BUFFER_LEN);
				}

			}
			
			free_fields(users, keyCount);
		}

		LeaveCriticalSection(input->GameSetup);
		Sleep(10);
	}
	printf("[Ready counter] ended thread...\n");
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