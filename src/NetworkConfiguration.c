#include "NetworkConfiguration.h"
#include <tools_c/Logging.h>
#include <tools_c/Repeat.h>
#include <tools_c/StringEquals.h>
#include <tools_c/ZeroMemory.h>
#include <ctype.h>
#include <stdlib.h>

enum
{
    protocol_buffer_size = 20
};

static void stringToLower(char* str)
{
    const int len = strlen(str);
    repeat(len) str[i] = tolower(str[i]);
}

static NetworkProtocol getProtocol(const char* str)
{
    char str_lower[protocol_buffer_size];
    strcpy(str_lower, str);
    stringToLower(str_lower);
    if (equals(str_lower, "tcp"))
        return TCP;
    if (equals(str_lower, "udp"))
        return UDP;
    if (equals(str_lower, "udp_lite"))
        return UDP_Lite;
    if (equals(str_lower, "dccp"))
        return DCCP;
    if (equals(str_lower, "unix"))
        return Unix;
    if (equals(str_lower, "fork"))
        return UnixForked;
    return SCTP;
}

NetworkConfiguration createNetworkConfiguration(const Args* args)
{
    NetworkConfiguration ret;
    ZERO_MEMORY(&ret);

    IPSocket* filling = ret.locals.sockets;
    Count* counter = &ret.locals.count;
    for (int i = 1; i < args->argc - 1; ++i)
    {
        const char* arg = args->argv[i];

        if (arg[0] == '-')
        {
            if (equals(arg, "-p"))
                ret.protocol = getProtocol(args->argv[++i]);
            else if (equals(arg, "-mtu"))
                ret.path_mtu = atoi(args->argv[++i]);
            else if (equals(arg, "-seg"))
                ret.max_seg = atoi(args->argv[++i]);
            else if (equals(arg, "-msg"))
                ret.big_msg_size = atoi(args->argv[++i]);
            else if (equals(args->argv[i], "-r"))
            {
                filling = ret.remotes.sockets;
                counter = &ret.remotes.count;
            }
        }
        else
        {
            IPSocket* filling_current = filling + (*counter)++;
            const char* arg_next = args->argv[i++ + 1];
            *filling_current = createIPSocket(arg, atoi(arg_next));
        }
    }

    return ret;
}
