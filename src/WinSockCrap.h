#pragma once

#include <ws2sctp.h>

typedef ADDRESS_FAMILY sa_family_t;

int close(int);

#define SOL_SCTP 0

#define F_GETFL 0
#define F_SETFL 0
#define O_NONBLOCK 0
int fcntl(int, int, int);
