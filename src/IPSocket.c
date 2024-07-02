#include "IPSocket.h"
#include "SocketErrorChecks.h"

static bool isFamily(const IP addr, Family family)
{
    struct sockaddr_storage ignore_pton_result;
    return not failedInetpton(inet_pton(family, addr, &ignore_pton_result));
}

static Family detectFamily(const IP addr)
{
    return isFamily(addr, AF_INET) ? AF_INET : isFamily(addr, AF_INET6) ? AF_INET6 : AF_UNIX;
}

IPSocket createIPSocket(const IP addr, Port port)
{
    IPSocket ret;
    memcpy(ret.addr, addr, ip_addr_size);
    ret.port = port;
    ret.family = detectFamily(addr);
    return ret;
}

IPSocket sockaddrToIPSocket(const struct sockaddr* saddr)
{
    return sockaddrStoragetoIPSocket((const struct sockaddr_storage*)saddr);
}

IPSocket sockaddrStoragetoIPSocket(const struct sockaddr_storage* saddr_storage)
{
    IPSocket ret;

    const void* sin_addr;

#define FILL_FROM_SADDR_STORAGE(Suffix)                                                                \
    {                                                                                                  \
        const struct sockaddr_in##Suffix* saddr_in = (const struct sockaddr_in##Suffix*)saddr_storage; \
        sin_addr = &saddr_in->sin##Suffix##_addr;                                                      \
        ret.port = ntohs(saddr_in->sin##Suffix##_port);                                                \
    }

    ret.family = saddr_storage->ss_family;
    if (isIPv6(&ret))
        FILL_FROM_SADDR_STORAGE(6)
    else
        FILL_FROM_SADDR_STORAGE()

    checkInetntop(inet_ntop(ret.family, sin_addr, ret.addr, ip_addr_size));

    return ret;
}

#define RETURN_SOCKADDR_IN(Suffix)                                                                          \
    struct sockaddr_in##Suffix ret = {0};                                                                   \
    ret.sin##Suffix##_family = ip_socket->family;                                                           \
    ret.sin##Suffix##_port = htons(ip_socket->port);                                                        \
    checkInetpton(inet_pton(ip_socket->family, ip_socket->addr, &ret.sin##Suffix##_addr.s##Suffix##_addr)); \
    return ret;

static struct sockaddr_in6 ipSocketToSockaddrIn6(const IPSocket* ip_socket)
{
    RETURN_SOCKADDR_IN(6);
}

static struct sockaddr_in ipSocketToSockaddrIn(const IPSocket* ip_socket)
{
    RETURN_SOCKADDR_IN();
}

struct sockaddr_storage ipSocketToSockaddrStorage(const IPSocket* ip_socket)
{
#define RETURN_SOCKADDR_STORAGE(Suffix)                                                   \
    {                                                                                     \
        const struct sockaddr_in##Suffix saddr = ipSocketToSockaddrIn##Suffix(ip_socket); \
        struct sockaddr_storage saddr_storage = {0};                                      \
        return saddr_storage = *(const struct sockaddr_storage*)&saddr;                   \
    }

    if (isIPv6(ip_socket))
        RETURN_SOCKADDR_STORAGE(6)
    else
        RETURN_SOCKADDR_STORAGE()
}

bool isIPv6(const IPSocket* ip_socket)
{
    return ip_socket->family == AF_INET6;
}

Size sizeofSockaddr(const IPSocket* ip_socket)
{
    return isIPv6(ip_socket) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);
}

Sockaddrs ipSocketsToSockaddrs(const IPSockets* ip_sockets)
{
    Byte* ret = {0};
    Count byte_count = 0;
    for (Count i = 0; i < ip_sockets->count; ++i)
    {
        const IPSocket* ip_socket = ip_sockets->sockets + i;
        const Size sizeof_current_ip_socket = sizeofSockaddr(ip_socket);
        byte_count += sizeof_current_ip_socket;
        ret = realloc(ret, byte_count);
        const struct sockaddr_storage saddr_storage = ipSocketToSockaddrStorage(ip_socket);
        memcpy(ret + byte_count - sizeof_current_ip_socket, &saddr_storage, sizeof_current_ip_socket);
    }
    return (Sockaddrs)ret;
}
