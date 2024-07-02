#pragma once

#ifndef _WIN32
#define __USE_MISC
#define __USE_GNU
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#else
#include "WinSockCrap.h"
#endif
