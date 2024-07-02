#pragma once

#include "Socket.h"

void bindSocketUnix(FD);
void connectSocketUnix(Socket*, const RemoteIPSockets*);
void handleMessageUnix(Socket*, FD);
void sendSocketUnix(Socket*, ChatMessage);
