#pragma once

#include "Socket.h"

void handleMessageDccp(Socket*, FD);
void sendSocketDccp(Socket*, ChatMessage);
