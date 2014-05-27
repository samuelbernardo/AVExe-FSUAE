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
#include <stdint.h>
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
		cerr << "Client: Cannot open socket! Socket number is " << listenFd << endl;
		return;
	}

	bzero((char *) &svrAdd, sizeof(svrAdd));
	svrAdd.sun_family = PF_UNIX;
	strcpy(svrAdd.sun_path, SOCKET_PATH);
	len = strlen(svrAdd.sun_path) + sizeof(svrAdd.sun_family);

	int checker = connect(listenFd,(struct sockaddr *) &svrAdd, sizeof(svrAdd));

	if (checker < 0)
	{
		cerr << "Client: Cannot connect! Socket number is " << listenFd << endl;
		return;
	}

}

void writeServer(uaecptr addr, uae_u32 data) {
	int checker, op;
	/*
	 * Verify if connection to server is already started
	 */
	if(!connection_state)
		startConnection();

	/*
	 * Send stuff to memory server
	 */
	//memPDU memoryBank;
	//memoryBank.id = id;
	op = MEMSERVER_WRITE;

	checker = write(listenFd, &op, sizeof(int));
	if (checker < 0)
	{
		cerr << "client writeServer: write error in op!" << endl;
		exit(checker);
	}
	checker = write(listenFd, &id, sizeof(int));
	if (checker < 0)
	{
		cerr << "client writeServer: write error in id!" << endl;
		exit(checker);
	}
	checker = write(listenFd, &addr, sizeof(int));
	if (checker < 0)
	{
		cerr << "client writeServer: write error in addr!" << endl;
		exit(checker);
	}
	checker = write(listenFd, &data, sizeof(int));
	if (checker < 0)
	{
		cerr << "client writeServer: write error in data!" << endl;
		exit(checker);
	}

}

uae_u32 readServer(uaecptr addr){
	int checker, op;
	uae_u32 data;
	/*
	 * Verify if connection to server is already started
	 */
	if(!connection_state)
		startConnection();

	/*
	 * Receive stuff from memory server
	 */
	//memPDU memoryBank;
	//memoryBank.id = id;
	op = MEMSERVER_READ;
	data = 0;

	checker = write(listenFd, &op, sizeof(int));
	if (checker < 0)
	{
		cerr << "client readServer: write error in op!" << endl;
		exit(checker);
	}
	checker = write(listenFd, &id, sizeof(int));
	if (checker < 0)
	{
		cerr << "client readServer: write error in id!" << endl;
		exit(checker);
	}
	checker = write(listenFd, &addr, sizeof(int));
	if (checker < 0)
	{
		cerr << "client readServer: write error in addr!" << endl;
		exit(checker);
	}

	checker = read(listenFd, (int*)&data, sizeof(int));
	if (checker < 0)
	{
		cerr << "client readServer: read error in data!" << endl;
		exit(checker);
	}

	return data;
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
