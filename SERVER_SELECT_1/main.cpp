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
#include <list>
#include <cstring>
#include "time.h"
#include <stdlib.h>
#include <ctime>

using namespace std;

const unsigned int LISTEN_PORT = 8082;
const unsigned short PORT_STRING_LEN = 7; // 6 digits for port + 1 for NULL.
const unsigned short SELECT_TIMEOUT_SEC = 5;

const char* REQUEST_WORD_TIME = "time\r\n";
const char* REQUEST_WORD_FILE = "file\r\n";

bool g_should_exit = false;

std::list<int> rd_conns; // List of sockets representing clients from which we're waiting for data.
std::list<int> wr_conns; // List of seckets representing clients to whom we want to send data.
int n_max_sock = 0;

void handleNewConnection(int fd){
    // There is a pending connection. Trying to accept.
    cout << "Pending connection available. Trying to accept..." << endl;
    int new_conn = accept4(fd, NULL, NULL, O_NONBLOCK);

    if (new_conn == -1){
        // Error. No problem with that. Just output the message and keep going.
        cout << "Error accepting the connection. errno = " << errno << endl;
        return;
    }

    cout << "Successfully accepted the pending connection. Adding to the rd_conns list." << endl;

    rd_conns.push_back(new_conn);
}

void handleRead(int fd){
    char buf[1024];

    int ret = recv(fd, (void*)buf, 1024, 0);
    if (ret == -1){
        cout << "recv returned error. errno = " << errno << endl;
    } else if (ret == 0) {
        cout << "no data received." << endl;
    } else {
        buf[ret] = 0;
        cout << "Received " << ret << " bytes. Value = \"" << buf << "\"" << endl;
    }

    return;
}

void handleWrite(int fd){
}

// TODO: Close sockets on ERROR.
// TODO: If connection is closed, socket must be romeved from
//       all sockets lists.

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
    //cout << "3. Putting socket to non-blocking mode..." << endl;

    //int flags = fcntl(l_sock, F_GETFL, 0);
    //fcntl(l_sock, F_SETFL, flags | O_NONBLOCK);

    // Setting Ctrl+C signal handler.
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = [](int s)->void {
        g_should_exit = true;
    };

    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);

	// Listen
	cout << "4. Starting to listen..." << endl;

	res = listen(l_sock, 0);

    if (res != 0) {
        cout << "Failed to switch socket to listenning mode! Error = "<< errno << "." << endl;
        return 0;
    }

    while (true) {
        // Accept
        cout << "5. Accepting next incoming connection..." << endl;
        int d_sock = accept(l_sock, NULL, NULL);

        if (d_sock == -1) {
            if (errno == EINTR) {
                cout << "Accept was interrupted by a signal!!!" << endl;
            }

            break;
        }

        cout << "Client has connected! Waiting for the request..." << endl;

        const int INPUT_BUFFER_SIZE_BYTES = 1024;
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
        // time - return current timestamp.
        // file - send content of a big file (5MB).
        // other - return 'bad request'.

        // Job done, closing the connection.
        shutdown(d_sock, SHUT_RDWR);
        close(d_sock);
    }

	// Loop
//	cout << "5. Entering select loop... Press Ctrl+C to exit." << endl;
//
//	while (true) {
//        fd_set rdfs, wdfs, edfs;
//        FD_ZERO(&rdfs);
//        FD_ZERO(&wdfs);
//        FD_ZERO(&edfs);
//
//        FD_SET(l_sock, &rdfs);
//
//        n_max_sock = l_sock;
//
//        for (auto & rs : rd_conns){
//            FD_SET(rs, &rdfs);
//            if (n_max_sock < rs)
//                n_max_sock = rs;
//        }
//
//        for (auto & ws: wr_conns) {
//            FD_SET(ws, &wdfs);
//            if (n_max_sock < ws)
//                n_max_sock = ws;
//        }
//
//        timeval select_time_out = {0};
//        select_time_out.tv_sec = SELECT_TIMEOUT_SEC;
//
//        res = select(n_max_sock + 1, &rdfs, &wdfs, &edfs, &select_time_out);
//
//        for (auto & rs: rd_conns){
//            if (FD_ISSET(rs, &rdfs))
//                handleRead(rs);
//        }
//
//        for (auto& ws: wr_conns){
//            if (FD_ISSET(ws, &wdfs))
//                handleWrite(ws);
//        }
//
//        if (FD_ISSET(l_sock, &rdfs) > 0) {
//            handleNewConnection(l_sock);
//        }
//        // Check if user wants to exit.
//        // TODO: Investigate if we need thread synchronization here. It's very important.
//        //       I think we don't, because there is one thread (the one that calls the signal handler)
//        //       sets this variable and the mani thread running this loop is the only thread that reads it.
//        //       I'm not sure if this variable an be in "broken" state. Anyway, gotta check it.
//        if (g_should_exit == true)
//            break;
//	}

    shutdown(l_sock, SHUT_RDWR);
    close(l_sock);

    cout << "Shutting down... Good Bye!!!" << endl;

	return 0;
}
