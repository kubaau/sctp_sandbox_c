#pragma once

#include "Socket.h"

void initForkedSocket(Socket*);
void handleMessageUnixForked(Socket*, FD);
void receiveUnixForked(Socket*);
void sendSocketUnixForked(Socket*, ChatMessage);
bool isChild(Socket*);
void cleanUpMessageQueues(Socket*);
void cleanUpSemaphores(Socket*);
void cleanUpSharedMemory(Socket*);
void cleanUpGlobals(void);
