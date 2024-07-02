#pragma once

#include <tools_c/TimeTypedefs.h>
#include "IPSocket.h"

typedef const char* ChatMessage;
typedef sctp_assoc_t AssocId;
typedef int FD;
typedef int SocketParam;

typedef IPSocket LocalIPSocket;
typedef IPSocket RemoteIPSocket;
typedef IPSockets LocalIPSockets;
typedef IPSockets RemoteIPSockets;

enum
{
    chat_buffer_size = 4096
};
typedef char ChatBuffer[chat_buffer_size];
