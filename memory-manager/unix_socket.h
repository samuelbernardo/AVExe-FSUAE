#ifndef __UNIX_SOCKET_H__
#define __UNIX_SOCKET_H__

/*struct sockaddr_un {
    unsigned short sun_family;   AF_UNIX
    char sun_path[108];
}*/

#define SOCKET_PATH "mem_socket"
#define MAX_CLIENTS 3
#define QUEUE_CLIENTS 5

#endif
