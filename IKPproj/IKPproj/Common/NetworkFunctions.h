#pragma once

#include <winsock2.h>
#include "FunctionCodes.h"

int SendBuffer(SOCKET sendSocket, const char* buff, int length);
int RecieveBuffer(SOCKET s, char* recieveBuffer, int length);
int CustomSelect(SOCKET s, OperationCodes op);
bool AcceptSocket(SOCKET s, SOCKET* acceptedSocket);