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
#include <assert.h>
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
    close(listenFd);
}

int main(int argc, char* argv[])
{
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

    //set socket options
    socket_option_z = setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR,
    		&so_reuseaddr, sizeof so_reuseaddr);

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
			break;
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

	int memSize = memoryStorage.memoryStorageSize();
	int allRequests = memoryStorage.memoryStatsSize();
	assert(allRequests!=0);
	int optimization = memSize/allRequests;

    ofstream logFile;
    logFile.open("log.txt");
    logFile << "memoryStorage: unordered_map<memoryID,uae_u32,memIDhash,memIDeqKey> database_type" << endl;
	for (database_type_iterator iter = memoryStorage.memoryStorageBeginIterator(); iter != memoryStorage.memoryStorageEndIterator(); iter++) {
		logFile << "Key: [" << iter->first.id << "," << iter->first.addr << "]\t\tData: " << iter->second << endl;
	}
	logFile << endl;
	logFile << endl;
	logFile << "memoryStats: unordered_multimap<uae_u32,memoryID> stats_type" << endl;
	for (stats_type_iterator iter = memoryStorage.memoryStatsBeginIterator(); iter != memoryStorage.memoryStatsEndIterator(); iter++) {
		logFile << "Key: "<< iter->first << "\t\tData: [" << iter->second.id << "," << iter->second.addr << "]" << endl;
	}

	cout << "memoryStorage.size() is " << memSize << endl;
	cout << "memoryStats.size() is " << allRequests << endl;
	cout << "Optimization acquired in run was: " << optimization << endl;

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
		memoryBank.data = memoryStorage.getMemoryData(memoryBank.addr, memoryBank.id);
		write(myConnFd, &memoryBank, sizeof(memPDU));
	}
	else {
		memoryStorage.putMemoryData(memoryBank.addr, memoryBank.id, memoryBank.data);
	}

    cout << "\nClosing thread and conn" << endl;
    close(myConnFd);

}
