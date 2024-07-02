#include "NetworkProtocol.h"
#include <iso646.h>
#include "SocketApi.h"

bool isUdp(NetworkProtocol protocol)
{
    return protocol == UDP or protocol == UDP_Lite;
}

bool isUnix(NetworkProtocol protocol)
{
    return protocol == Unix or protocol == UnixForked;
}

SocketType typeFromNetworkProtocol(NetworkProtocol protocol)
{
    switch (protocol)
    {
        case TCP: return SOCK_STREAM;
        case UDP:
        case UDP_Lite:
        case Unix:
        case UnixForked: return SOCK_DGRAM;
        case DCCP: return SOCK_DCCP;
        default: return SOCK_SEQPACKET;
    }
}

int protocolFromNetworkProtocol(NetworkProtocol protocol)
{
    switch (protocol)
    {
        case UDP_Lite: return IPPROTO_UDPLITE;
        default: return 0;
    }
}
