#ifndef __CLIENT_H__
#define __CLIENT_H__

#include "unix_socket.h"
#include "MemoryStorage.h"

void writeServer(uaecptr addr, uae_u32 data);

uae_u32 readServer(uaecptr addr);

#endif
