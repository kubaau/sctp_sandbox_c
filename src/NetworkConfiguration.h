#pragma once

#include <tools_c/Args.h>
#include "NetworkProtocol.h"
#include "Typedefs.h"

enum
{
    locals_max_count = 4,
    remotes_max_count = locals_max_count
};

typedef struct
{
    LocalIPSockets locals;
    RemoteIPSockets remotes;
    NetworkProtocol protocol;
    SocketParam path_mtu;
    SocketParam max_seg;
    Size big_msg_size;
} NetworkConfiguration;

NetworkConfiguration createNetworkConfiguration(const Args*);
