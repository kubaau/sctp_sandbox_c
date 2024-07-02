#pragma once

#include "Socket.h"

void bindSocketTcp(FD, const LocalIPSocket*);
void connectSocketTcp(Socket*, const RemoteIPSockets*);
void handleMessageTcp(Socket*, FD);
void handleCommUpTcp(Socket*, FD);
void handleCommLostTcp(Socket*, FD);
void handleGracefulShutdownTcp(Socket*, FD);
void sendSocketTcp(Socket*, ChatMessage);
void logPaddrsTcp(FD);
