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
#include "unix_socket.h"

using namespace std;

int main (int argc, char* argv[])
{
    int listenFd, portNo, len;
    bool loop = false;
    struct sockaddr_un svrAdd;
    //struct hostent *server;

    /*if(argc < 3)
    {
        cerr<<"Syntax : ./client <host name> <port>"<<endl;
        return 0;
    }*/

    //portNo = strtol(argv[2],NULL,10);

    /*if((portNo > 65535) || (portNo < 2000))
    {
        cerr<<"Please enter port number between 2000 - 65535"<<endl;
        return 0;
    }*/

    //create client skt
    //listenFd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    listenFd = socket(PF_UNIX, SOCK_STREAM, 0);

    if(listenFd < 0)
    {
        cerr << "Cannot open socket" << endl;
        return 0;
    }

    /*server = gethostbyname(argv[1]);

    if(server == NULL)
    {
        cerr << "Host does not exist" << endl;
        return 0;
    }*/

    bzero((char *) &svrAdd, sizeof(svrAdd));
    //svrAdd.sin_family = PF_INET;
    svrAdd.sun_family = PF_UNIX;
	strcpy(svrAdd.sun_path, SOCKET_PATH);
	len = strlen(svrAdd.sun_path) + sizeof(svrAdd.sun_family);

    //bcopy((char *) server -> h_addr, (char *) &svrAdd.sin_addr.s_addr, server -> h_length);

    //svrAdd.sin_port = htons(portNo);

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
        //cin.clear();
        //cin.ignore(256, '\n');
        cout << "Enter stuff: ";
        bzero(s, 300);
        cin.getline(s, 300);

        /*if (send(listenFd, s, strlen(s), 0) == -1) {
			cerr << "send" << endl;
			exit(1);
		}

		if ((t=recv(listenFd, s, 300, 0)) > 0) {
			string answer(s);
			cout << "echo> " << answer << endl;
		} else {
			if (t < 0) cerr << "recv" << endl;
			else cerr << "Server closed connection" << endl;
			exit(1);
		}*/

        write(listenFd, s, strlen(s));

        cout << "Write finished. Waiting for answer from server..." << endl;
        read(listenFd, a, 300);
        string answer(a);
        cout << answer << endl;
    }
}
