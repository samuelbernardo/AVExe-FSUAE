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
#include "unix_socket.h"

using namespace std;

void *task1(void *);

//mutex mtx;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int listenFd, connFd[MAX_CLIENTS], noThread, noThreadLocal;

int main(int argc, char* argv[])
{
    //int pId, portNo;
    socklen_t len; //store size of the address
    bool loop = false;
    struct sockaddr_un svrAdd;

    pthread_t threadA[MAX_CLIENTS];

    /*if (argc < 2)
    {
        cerr << "Syntax : ./server <port>" << endl;
        return 0;
    }

    portNo = strtol(argv[1],NULL,10);

    if((portNo > 65535) || (portNo < 2000))
    {
        cerr << "Please enter a port number between 2000 - 65535" << endl;
        return 0;
    }*/

    //create socket
    //listenFd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    listenFd = socket(PF_UNIX, SOCK_STREAM, 0);

    if(listenFd < 0)
    {
        cerr << "Cannot open socket" << endl;
        return 0;
    }

    bzero((char*) &svrAdd, sizeof(svrAdd));

    //svrAdd.sin_family = PF_INET;
    //svrAdd.sin_addr.s_addr = INADDR_ANY;
    //svrAdd.sin_port = htons(portNo);
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

    for(noThreadLocal=0, noThread = 0; noThread < 3; noThreadLocal++)
    {
        cout << "Listening" << endl;
        struct sockaddr_un clntAdd;
		socklen_t len = sizeof(clntAdd);

		//this is where client connects. svr will hang in this mode until client conn
		connFd[noThread] = accept(listenFd, (struct sockaddr *)&clntAdd, &len);

		if (connFd[noThread] < 0)
		{
			cerr << "Cannot accept connection" << endl;
			return 0;
		}
		else
		{
			cout << "Connection successful" << endl;
		}

        pthread_create(&threadA[noThread], NULL, task1, NULL);

        while(noThreadLocal == noThread);
    }

    for(int i = 0; i < 3; i++)
    {
        pthread_join(threadA[i], NULL);
    }

}

void *task1 (void *dummyPt)
{
	//mtx.lock();
	int rc = pthread_mutex_lock(&mutex);
    int myThread = noThread;
    noThread++;
    //mtx.unlock();
    pthread_mutex_unlock(&mutex);

    cout << "Thread No: " << pthread_self() << endl;
    cout << "MyThread No: " << myThread << endl;
    char test[300];
    bool loop = false;
    while(!loop)
    {
        bzero(test, 300);

        /*int n = recv(connFd, test, 300, 0);
        if (n <= 0) {
            if (n < 0) perror("recv");
            loop = true;
        }

        if (!loop)
            if (send(connFd, test, n, 0) < 0) {
                perror("send");
                loop = true;
            }*/

        read(connFd[myThread], test, 300);

        string tester(test);
        cout << tester << endl;

        ostringstream oss;
        oss << "Server: " << tester << " -> message received with success!" << endl;
        string s = oss.str();
        write(connFd[myThread], s.c_str(), s.length());

        if(tester == "exit")
            break;
    }
    cout << "\nClosing thread and conn" << endl;
    close(connFd[myThread]);
}
