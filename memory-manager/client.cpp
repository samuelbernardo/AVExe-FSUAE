#include <string.h>
#include <cstring>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <strings.h>
#include <stdlib.h>
#include <string>
#include <time.h>
#include <vector>
#include <cstdlib>
#include "client.h"

using namespace std;

static int listenFd, len, id = rand() % RAND_MAX;
static struct sockaddr_un svrAdd;
static bool connection_state = false;

/*
 *  System Unix Socket
 */
void startConnection() {

    listenFd = socket(PF_UNIX, SOCK_STREAM, 0);

	if(listenFd < 0)
	{
		cerr << "Cannot open socket" << endl;
		return;
	}

	bzero((char *) &svrAdd, sizeof(svrAdd));
	svrAdd.sun_family = PF_UNIX;
	strcpy(svrAdd.sun_path, SOCKET_PATH);
	len = strlen(svrAdd.sun_path) + sizeof(svrAdd.sun_family);

	int checker = connect(listenFd,(struct sockaddr *) &svrAdd, sizeof(svrAdd));

	if (checker < 0)
	{
		cerr << "Cannot connect!" << endl;
		return;
	}

}

void writeServer(uaecptr addr, uae_u32 data) {

	/*
	 * Verify if connection to server is already started
	 */
	if(!connection_state)
		startConnection();

	/*
	 * Send stuff to memory server
	 */
	memPDU memoryBank;
	memoryBank.id = id;
	memoryBank.op = MEMSERVER_WRITE;
	memoryBank.addr = addr;
	memoryBank.data = data;

	write(listenFd, &memoryBank, sizeof(memPDU));

}

uae_u32 readServer(uaecptr addr){
	/*
	 * Verify if connection to server is already started
	 */
	if(!connection_state)
		startConnection();

	/*
	 * Receive stuff from memory server
	 */
	memPDU memoryBank;
	memoryBank.id = id;
	memoryBank.op = MEMSERVER_READ;
	memoryBank.addr = addr;
	memoryBank.data = 0;

	write(listenFd, &memoryBank, sizeof(memPDU));

	read(listenFd, &memoryBank, sizeof(memPDU));

	return memoryBank.data;
}


#if 0
int main (int argc, char* argv[])
{
    int listenFd, portNo, len;
    bool loop = false;
    struct sockaddr_un svrAdd;

    //create client skt
    listenFd = socket(PF_UNIX, SOCK_STREAM, 0);

    if(listenFd < 0)
    {
        cerr << "Cannot open socket" << endl;
        return 0;
    }

    bzero((char *) &svrAdd, sizeof(svrAdd));
    svrAdd.sun_family = PF_UNIX;
	strcpy(svrAdd.sun_path, SOCKET_PATH);
	len = strlen(svrAdd.sun_path) + sizeof(svrAdd.sun_family);

    int checker = connect(listenFd,(struct sockaddr *) &svrAdd, sizeof(svrAdd));

    if (checker < 0)
    {
        cerr << "Cannot connect!" << endl;
        return 0;
    }

    //send stuff to server
    for(;;)
    {
        char s[300], a[300];
        int t;
        cout << "Enter stuff: ";
        bzero(s, 300);
        cin.getline(s, 300);

        write(listenFd, s, strlen(s));

        cout << "Write finished. Waiting for answer from server..." << endl;
        read(listenFd, a, 300);
        string answer(a);
        cout << answer << endl;
    }
}
#endif
