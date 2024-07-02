#pragma once

#include <tools_c/TimeUtils.h>
#include "NetworkConfiguration.h"
#include "Socket.h"

typedef struct
{
    NetworkConfiguration config;
    const char* name;
    Duration delay;
    Duration lifetime;
} NetworkTaskParams;

int networkTask(void*);
