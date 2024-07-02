#pragma once

#include "Typedefs.h"

enum
{
    selected_fds_max_count = 10
};

typedef Milliseconds Timeout;

typedef struct
{
    fd_set fds;
    FD fd_max;

    FD selected_fds[selected_fds_max_count];
    Count selected_fds_count;
} FDSet;

void resetFdSet(FDSet*);
void setFd(FDSet*, FD);
void selectFdSet(FDSet*, Timeout);
