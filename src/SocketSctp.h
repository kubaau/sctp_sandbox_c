#pragma once

#include "Socket.h"

void configureSocketSctp(Socket*, FD);
void bindSocketSctp(Socket*, const LocalIPSockets*);
void connectSocketSctp(Socket*, const RemoteIPSockets*);
void handleMessageSctp(Socket*, FD);
void sendSocketSctp(Socket*, ChatMessage);
