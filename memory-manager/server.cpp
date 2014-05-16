#include <string.h>
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
#include <strings.h>
#include <stdlib.h>
#include <string>
#include <pthread.h>
//#include <condition_variable>
//#include <mutex>
//#include <algorithm>
//#include <thread>
//#include <queue>
//#include <chrono>
#include <errno.h>
#include <signal.h>
#include "server.h"

using namespace std;

void *task1(void *);

//mutex mtx;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int listenFd, connFd, noThread;

static MemoryStorage &memoryStorage = *(new MemoryStorage());

bool run = true;

void sighandler(int sig)
{
    cout<< "Signal " << sig << " caught..." << endl;

    run = false;
}

int main(int argc, char* argv[])
{
    //int pId, portNo;
    socklen_t len; //store size of the address
    int noThreadLocal;
    struct sockaddr_un svrAdd;

    pthread_t threadA;

    signal(SIGABRT, &sighandler);
    signal(SIGTERM, &sighandler);
    signal(SIGINT, &sighandler);

    //create socket
    listenFd = socket(PF_UNIX, SOCK_STREAM, 0);

    if(listenFd < 0)
    {
        cerr << "Cannot open socket" << endl;
        return 0;
    }

    bzero((char*) &svrAdd, sizeof(svrAdd));

    svrAdd.sun_family = PF_UNIX;
    strcpy(svrAdd.sun_path, SOCKET_PATH);
    unlink(svrAdd.sun_path);
    len = strlen(svrAdd.sun_path) + sizeof(svrAdd.sun_family);

    //bind socket
    if(bind(listenFd, (struct sockaddr *)&svrAdd, sizeof(svrAdd)) < 0)
    {
        cerr << "Cannot bind" << endl;
        return 0;
    }

    listen(listenFd, QUEUE_CLIENTS);

    for(run=true, noThreadLocal=0, noThread = 0; run; noThreadLocal++)
    {
        cout << "Listening" << endl;
        struct sockaddr_un clntAdd;
		socklen_t len = sizeof(clntAdd);

		//this is where client connects. svr will hang in this mode until client conn
		connFd = accept(listenFd, (struct sockaddr *)&clntAdd, &len);

		if (connFd < 0)
		{
			cerr << "Cannot accept connection" << endl;
			return 0;
		}
		else
		{
			cout << "Connection successful" << endl;
		}

        pthread_create(&threadA, NULL, task1, NULL);
        pthread_join(threadA, NULL);

        // TODO: very inefficient
        while(noThreadLocal == noThread);
    }

#if 0
    for(int i = 0; i < noThreadLocal; i++)
    {
        pthread_join(threadA[i], NULL);
    }
#endif

    cout << "Server terminated!" << endl;

}

void *task1 (void *dummyPt)
{
	//mtx.lock();
	int rc = pthread_mutex_lock(&mutex);
    int myThread = noThread;
    int myConnFd = connFd;
    noThread++;
    //mtx.unlock();
    pthread_mutex_unlock(&mutex);

    memPDU memoryBank;

    cout << "Thread No: " << pthread_self() << endl;
    cout << "MyThread No: " << myThread << endl;

	read(myConnFd, &memoryBank, sizeof(memPDU));

	if(memoryBank.op == MEMSERVER_READ) {
		//memoryBank.data = memoryStorage.getMemoryData(memoryBank.addr, memoryBank.id);
		write(myConnFd, &memoryBank, sizeof(memPDU));
	}
	else {
		//memoryStorage.putMemoryData(memoryBank.addr, memoryBank.id, memoryBank.data);
	}

    cout << "\nClosing thread and conn" << endl;
    close(myConnFd);

}
