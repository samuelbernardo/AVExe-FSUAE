#ifndef __UNIX_SOCKET_H__
#define __UNIX_SOCKET_H__

#include "MemoryStorage.h"

/*struct sockaddr_un {
    unsigned short sun_family;   AF_UNIX
    char sun_path[108];
}*/

#define SOCKET_PATH "mem_socket"
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
