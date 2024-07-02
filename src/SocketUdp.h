#pragma once

#include "Socket.h"

void connectSocketUdp(Socket*, const RemoteIPSockets*);
void handleMessageUdp(Socket*, FD);
void sendSocketUdp(Socket*, ChatMessage);
