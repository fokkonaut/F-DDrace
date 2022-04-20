#ifndef ENGINE_SERVER_UDP_H
#define ENGINE_SERVER_UDP_H
#include <base/system.h>

int SendPacketUDP(NETADDR *pSource, NETADDR *pDestination, void *pData, int Amount = 1);

#endif
