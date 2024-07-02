#pragma once

#include <tools_c/Typedefs.h>
#include <stdbool.h>
#include "SocketApi.h"

enum
{
    ip_addr_size = INET6_ADDRSTRLEN,
    any_port = 0
};
typedef char IP[ip_addr_size];
typedef unsigned short Port;
typedef sa_family_t Family;

typedef struct
{
    IP addr;
    Port port;
    Family family;
} IPSocket;

enum
{
    max_ip_socket_count = 4
};

typedef struct
{
    IPSocket sockets[max_ip_socket_count];
    Count count;
} IPSockets;

IPSocket createIPSocket(const IP, Port);
IPSocket sockaddrToIPSocket(const struct sockaddr*);
IPSocket sockaddrStoragetoIPSocket(const struct sockaddr_storage*);
struct sockaddr_storage ipSocketToSockaddrStorage(const IPSocket*);
bool isIPv6(const IPSocket*);
Size sizeofSockaddr(const IPSocket*);

typedef struct sockaddr* Sockaddrs;
Sockaddrs ipSocketsToSockaddrs(const IPSockets*);
