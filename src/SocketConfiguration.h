#pragma once

#include "Constants.h"
#include "Typedefs.h"

typedef SocketParam InitMaxAttempts;
typedef SocketParam InitMaxTimeoutInMs;
typedef SocketParam RtoInitialInMs;
typedef SocketParam RtoMinInMs;
typedef SocketParam RtoMaxInMs;
typedef SocketParam SackFreq;
typedef SocketParam SackDelay;
typedef SocketParam HbInterval;
typedef SocketParam PathMaxRetrans;
typedef SocketParam PathMtu;
typedef SocketParam KeepAliveTimeInS;
typedef SocketParam KeepAliveIntervalInS;
typedef SocketParam KeepAliveProbes;

enum
{
    all_associations = ignore_assoc
};

void configureNoDelay(FD);
void configureReuseAddr(FD);
void configureReceivingEvents(FD);
void configureInitParams(FD, InitMaxAttempts, InitMaxTimeoutInMs);
void configureRto(FD, RtoInitialInMs, RtoMinInMs, RtoMaxInMs, AssocId);
void configureSack(FD, SackFreq, SackDelay, AssocId);
void configureMaxRetrans(FD, SocketParam, AssocId);
void configureDscp(FD, Byte, Family);
void configureIPv4AddressMapping(FD);
void configureTTL(FD, SocketParam);
void configurePeerAddressParams(FD, HbInterval, PathMaxRetrans, PathMtu, Family, AssocId);
void configureMaxSeg(FD, SocketParam, AssocId);
void configureNonBlockingMode(FD);
void configureKeepAlive(FD, KeepAliveTimeInS, KeepAliveIntervalInS, KeepAliveProbes);
void configurePassCred(FD);
