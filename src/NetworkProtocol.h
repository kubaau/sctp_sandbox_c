#pragma once

#include <stdbool.h>

typedef enum
{
    SCTP,
    TCP,
    UDP,
    UDP_Lite,
    DCCP,
    Unix,
    UnixForked
} NetworkProtocol;

bool isUdp(NetworkProtocol);
bool isUnix(NetworkProtocol);

typedef int SocketType;
SocketType typeFromNetworkProtocol(NetworkProtocol);

int protocolFromNetworkProtocol(NetworkProtocol);
