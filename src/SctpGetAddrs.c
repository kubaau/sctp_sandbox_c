#include "SctpGetAddrs.h"
#include <tools_c/Logging.h>
#include <tools_c/PtrMagic.h>
#include <tools_c/Repeat.h>
#include "SocketErrorChecks.h"

typedef enum
{
    AddrType_LOCAL,
    AddrType_PEER
} AddrType;

static void logAddrs(FD fd, AssocId assoc_id, AddrType type)
{
    struct sockaddr* saddrs;
    int count = checkedGetaddrs(type == AddrType_LOCAL ? sctp_getladdrs(fd, assoc_id, &saddrs) :
                                                         sctp_getpaddrs(fd, assoc_id, &saddrs));
    struct sockaddr* current_saddr = saddrs;
    repeat(count)
    {
        const IPSocket ip_socket = sockaddrToIPSocket(current_saddr);
        DEBUG_LOG("%s:%d", ip_socket.addr, ip_socket.port);
        current_saddr = moveNBytes(current_saddr, sizeofSockaddr(&ip_socket));
        // we're iterating through an array of sockaddr's, but this array actually contains sockaddr_in's
        // and sockaddr_in6's - to get the next element of the array, we need to push the pointer forwards n bytes,
        // where n is the size of either sockaddr_in or sockaddr_in6 (depending on the current element)
    }
    if (count > 0) // addrs is undefined if get returns 0 or less
        type == AddrType_LOCAL ? sctp_freeladdrs(saddrs) : sctp_freepaddrs(saddrs);
}

void logLaddrs(FD fd, AssocId assoc_id)
{
    logAddrs(fd, assoc_id, AddrType_LOCAL);
}

void logPaddrs(FD fd, AssocId assoc_id)
{
    logAddrs(fd, assoc_id, AddrType_PEER);
}
