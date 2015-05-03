
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
#include <list>
#include <cstring>
#include "time.h"
#include <stdlib.h>
#include <ctime>

#include "settings.h"
#include "ST_sync.h"

using namespace std;

bool g_should_exit1 = false;

bool STSync::Init(){
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
		return false;
	}
#endif

	cout << "1. Creating listen socket..." << endl;
	m_l_sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_l_sock == -1) {
#ifdef WIN32
		DWORD lastErr = GetLastError();
		cout << "Error creating listen socket. LastErr = " << lastErr << endl;
#else
		cout << "Error creating listen socket. errno = " << errno << endl;
#endif
		return false;
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

        return false;
    }

    for (addrinfo* p_addr = p_l_addr_info; p_addr != NULL; p_addr = p_addr->ai_next) {
        res = bind(m_l_sock, p_addr->ai_addr, p_addr->ai_addrlen);

        if (res == 0) {
            cout << "Successfully bound to port " << s_l_port << "." << endl;
            break;
        }

        cout << "Could not bind to the address. Errno = " << errno << ". Trying next address..." << endl;
    }

    if (res != 0) {
        cout << "Faild to bind the socket!" << endl;
        return false;
    }

    // SetSockOpt - non-blocking
    //cout << "3. Putting socket to non-blocking mode..." << endl;

    //int flags = fcntl(l_sock, F_GETFL, 0);
    //fcntl(l_sock, F_SETFL, flags | O_NONBLOCK);

    // Setting Ctrl+C signal handler.
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = [](int s)->void {
        g_should_exit1 = true;
    };

    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);

    return true;
}

bool STSync::Start(){

	// Listen
	cout << "4. Starting to listen..." << endl;

	int res = listen(m_l_sock, 0);

    if (res != 0) {
        cout << "Failed to switch socket to listenning mode! Error = "<< errno << "." << endl;
        return false;
    }

    while (true) {
        // Accept
        cout << "5. Accepting next incoming connection..." << endl;
        int d_sock = accept(m_l_sock, NULL, NULL);

        if (d_sock == -1) {
            if (errno == EINTR) {
                cout << "Accept was interrupted by a signal!!!" << endl;
            }

            break;
        }

        cout << "Client has connected! Waiting for the request..." << endl;

        char in_buf[INPUT_BUFFER_SIZE_BYTES];

        int bytes_recv = recv(d_sock, static_cast<void*>(&in_buf), INPUT_BUFFER_SIZE_BYTES, 0);

        if (bytes_recv == -1) {
            cout << "Recv failed! Errno = " << errno << endl;
            shutdown(d_sock, SHUT_RDWR);
            close(d_sock);
            break;
        }

        cout << "Received data from client: " << in_buf << endl;

        // Processing the request.
        if (strcmp(REQUEST_WORD_TIME, in_buf) == 0) {
            const unsigned int TIMESTAMP_STR_SIZE_BYTES = 200;
            char timestamp[TIMESTAMP_STR_SIZE_BYTES];

            time_t t = time(NULL);
            tm* loc_time = localtime(&t);

            int timestamp_str_size = strftime(timestamp, sizeof(timestamp), "%a, %d %b %Y %T %z", loc_time);

            int bytes_sent = send(d_sock,static_cast<void*>(&timestamp), timestamp_str_size, 0);

            if (bytes_sent == -1) {
                cout << "Failed to send timestamp to the client! Errno = " << errno << endl;
                shutdown(d_sock, SHUT_RDWR);
                close(d_sock);
                break;
            }
        } else if (strcmp(REQUEST_WORD_FILE, in_buf) == 0) {
            const char buf[] = "TODO: Send big file to the client.";

            int bytes_sent = send(d_sock,static_cast<const void*>(&buf), sizeof(buf), 0);

            if (bytes_sent == -1) {
                cout << "Failed to send content of a file to the client! Errno = " << errno << endl;
                shutdown(d_sock, SHUT_RDWR);
                close(d_sock);
                break;
            }
        } else {
            const char buf[] = "Unknown request";

            int bytes_sent = send(d_sock,static_cast<const void*>(&buf), sizeof(buf), 0);

            if (bytes_sent == -1) {
                cout << "Failed to send timestamp to the client! Errno = " << errno << endl;
                shutdown(d_sock, SHUT_RDWR);
                close(d_sock);
                break;
            }
        }

        // Job done, closing the connection.
        shutdown(d_sock, SHUT_RDWR);
        close(d_sock);
    }

    shutdown(m_l_sock, SHUT_RDWR);
    close(m_l_sock);

    cout << "Shutting down... Good Bye!!!" << endl;

    return true;
}
