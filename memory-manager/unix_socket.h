#ifndef __UNIX_SOCKET_H__
#define __UNIX_SOCKET_H__

#include "MemoryStorage.h"

// Definições principais para a comunicação
#define SOCKET_PATH "/tmp/mem_socket_fsuae"
#define MAX_CLIENTS 3
#define QUEUE_CLIENTS 5

#define MEMSERVER_WRITE 1
#define MEMSERVER_READ 2

typedef struct {
	int op;
	int id;
	uaecptr addr;
	uae_u32 data;
} memPDU;


#endif
