// ConsoleApplication1.cpp : Defines the entry point for the console application.
//

//#define WIN32 1

#ifdef WIN32
    #include "stdafx.h"
    #include "winsock.h"
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <netdb.h>
    #include <errno.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <signal.h>
    #include <cstdlib>
#endif // WIN32

#include <iostream>
#include <cstdio>

using namespace std;

const unsigned int LISTEN_PORT = 8082;
const unsigned short PORT_STRING_LEN = 7; // 6 digits for port + 1 for NULL.
const unsigned short SELECT_TIMEOUT_SEC = 5;

bool g_should_exit = false;

// TODO: Close sockets on ERROR.

int main()
{
	int res = 0;
#ifdef WIN32
	DWORD lastErr = 0;
#endif

	cout << "Welcome to a Web Server based on select function." << endl;
	cout << "Initializing:" << endl;

	// TODO: Do we need to initialize the socket library like on Windows calling WSAStartup()?
#ifdef WIN32
	WORD wVersionRequested = MAKEWORD(2, 2);
	WSADATA WSAData;
	res = WSAStartup(wVersionRequested, &WSAData);
	if (res != 0)
	{
		cout << "WSAStartup failed!. LastErr = " << res << endl;
		return 0;
	}
#endif

	cout << "1. Creating listen socket..." << endl;
	auto l_sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (l_sock == -1) {
#ifdef WIN32
		DWORD lastErr = GetLastError();
		cout << "Error creating listen socket. LastErr = " << lastErr << endl;
#else
		cout << "Error creating listen socket. errno = " << errno << endl;
#endif
		return 0;
	}

	// Bind
	cout << "2. Binding socket to local network interface..." << endl;

	addrinfo* p_l_addr_info;

	addrinfo l_addr_info_hints = {0};
	l_addr_info_hints.ai_family = AF_INET;
	l_addr_info_hints.ai_socktype = SOCK_STREAM;
	l_addr_info_hints.ai_protocol = IPPROTO_TCP;
	l_addr_info_hints.ai_flags = AI_PASSIVE;

	char s_l_port[PORT_STRING_LEN];
	sprintf(s_l_port, "%d", LISTEN_PORT);

	res = getaddrinfo(NULL, s_l_port, &l_addr_info_hints, &p_l_addr_info);

    if (res != 0){
        cout << "getaddrinfo() failed! Status: " << gai_strerror(res);
        if (res == EAI_SYSTEM)
            cout << "errno = " << errno << ".";

        cout << endl;

        return 0;
    }

    for (addrinfo* p_addr = p_l_addr_info; p_addr != NULL; p_addr = p_addr->ai_next) {
        res = bind(l_sock, p_addr->ai_addr, p_addr->ai_addrlen);

        if (res == 0) {
            cout << "Successfully bound to port " << s_l_port << "." << endl;
            break;
        }

        cout << "Could not bind to the address. Errno = " << errno << ". Trying next address..." << endl;
    }

    if (res != 0) {
        cout << "Faild to bind the socket!" << endl;
        return 0;
    }

    // SetSockOpt - non-blocking
    cout << "3. Putting socket to non-blocking mode..." << endl;

    int flags = fcntl(l_sock, F_GETFL, 0);
    fcntl(l_sock, F_SETFL, flags | O_NONBLOCK);

	// Listen
	cout << "4. Starting to listen..." << endl;

	res = listen(l_sock, 0);

    if (res != 0) {
        cout << "Failed to switch socket to listenning mode! Error = "<< errno << "." << endl;
        return 0;
    }

// Setting Ctrl+C signal handler.
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = [](int s)->void {
        g_should_exit = true;
    };

    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);

	// Loop
	cout << "5. Entering select loop... Press Ctrl+C to exit." << endl;

	while (true) {
        fd_set rdfs, wdfs, edfs;
        FD_ZERO(rdfs);
        FD_ZERO(wdfs);
        FD_ZERO(edfs);

        FD_SET(l_sock, &rdfs);

        timeval select_time_out = {0};
        select_time_out.tv_sec = SELECT_TIMEOUT_SEC;

        int ndfs = l_sock + 1;

        res = select(ndfs, &rdfs, &wdfs, &edfs, &select_time_out);

        // Accept.


        // Check if user wants to exit.
        // TODO: Investigate if we need thread synchronization here. It's very important.
        //       I think we don't, because there is one thread (the one that calls the signal handler)
        //       sets this variable and the mani thread running this loop is the only thread that reads it.
        //       I'm not sure if this variable an be in "broken" state. Anyway, gotta check it.
        if (g_should_exit == true)
            break;
	}

    cout << "Shutting down... Good Bye!!!" << endl;

	return 0;
}