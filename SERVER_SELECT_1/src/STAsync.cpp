#include "../include/STAsync.h"

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

#include "../settings.h"

using namespace std;

bool g_should_exit = false;

bool STAsync::Init(){
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
    cout << "3. Putting socket to non-blocking mode..." << endl;

    int flags = fcntl(m_l_sock, F_GETFL, 0);
    fcntl(m_l_sock, F_SETFL, flags | O_NONBLOCK);

    // Setting Ctrl+C signal handler.
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = [](int s)->void {
        g_should_exit = true;
    };

    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);

    return true;
}

void STAsync::HandleNewConnection() {
    cout << "Handling new connection..." << endl;
    while (true) {
        int d_sock = accept4(m_l_sock, NULL, NULL, SOCK_NONBLOCK);

        if (d_sock == -1) {
            if (errno == EAGAIN ||
                errno == EWOULDBLOCK) {
                    cout << "No available connections in the queue." << endl;
                    break;
                } else {
                    // Error has occured.
                    cout << "Error while accepting new connection. Errno = " << errno << "." << std::endl;
                    break;
                }
        }

        cout << "Data socket has been created: " << d_sock << "." << std::endl;

        ConnectionDescriptor descr;
        descr.state = CS_READ_PENDING;

        m_connections.insert(std::pair<int, ConnectionDescriptor>(d_sock, descr)); // Add new item to the map.
    }
}

void STAsync::HandleRead(int sock){
    auto conn = m_connections.find(sock);
    if (conn == m_connections.end())
        return;

    int bytes_recv = recv(conn->first, static_cast<void*>(&(conn->second.in_buf)), INPUT_BUFFER_SIZE_BYTES, 0);

    if (bytes_recv == -1) {
        cout << "Recv failed! Errno = " << errno << endl;
        shutdown(conn->first, SHUT_RDWR);
        close(conn->first);
        m_connections.erase(conn);
        return;
    }

    cout << "Received data from client: " << conn->second.in_buf << endl;
    cout << "Processing the request..." << endl;

    // Processing the request.
    if (strcmp(REQUEST_WORD_TIME, conn->second.in_buf) == 0) {
        time_t t = time(NULL);
        tm* loc_time = localtime(&t);

        int time_size = strftime(conn->second.out_buf, sizeof(conn->second.out_buf), "%a, %d %b %Y %T %z", loc_time);
        conn->second.out_size_bytes = time_size;

    } else if (strcmp(REQUEST_WORD_FILE, conn->second.in_buf) == 0) {
        sprintf(conn->second.out_buf, "TODO: Send big file to the client.");
        conn->second.out_size_bytes = strlen(conn->second.out_buf);

    } else {
        sprintf(conn->second.out_buf, "Unknown request");
        conn->second.out_size_bytes = strlen(conn->second.out_buf);
    }

    conn->second.state = CS_WRITE_PENDING;
}

void STAsync::HandleWrite(int sock){
    auto conn = m_connections.find(sock);
    if (conn == m_connections.end())
        return;

    int bytes_sent = send(conn->first,static_cast<const void*>(&(conn->second.out_buf)), conn->second.out_size_bytes, 0);

    if (bytes_sent == -1) {
        cout << "Failed to send content of a file to the client! Errno = " << errno << endl;
    }

    shutdown(conn->first, SHUT_RDWR);
    close(conn->first);
    conn->second.state = CS_CLOSED;

    return;
}

bool STAsync::Start(){

	// Listen
	cout << "4. Starting to listen..." << endl;

	int res = listen(m_l_sock, 0);

    if (res != 0) {
        cout << "Failed to switch socket to listenning mode! Error = "<< errno << "." << endl;
        return false;
    }

    while (true) {
        fd_set rdfs, wdfs, edfs;
        FD_ZERO(&rdfs);
        FD_ZERO(&wdfs);
        FD_ZERO(&edfs);

        FD_SET(m_l_sock, &rdfs);

        int n_max_sock = m_l_sock;

        for (auto & conn : m_connections){
            if (conn.second.state == CS_READ_PENDING) {
                FD_SET(conn.first, &rdfs);
            }
            else if (conn.second.state == CS_WRITE_PENDING) {
                FD_SET(conn.first, &wdfs);
            }

            if (n_max_sock < conn.first)
                n_max_sock = conn.first;
        }

        timeval select_time_out = {0};
        select_time_out.tv_sec = SELECT_TIMEOUT_SEC;

        res = select(n_max_sock + 1, &rdfs, &wdfs, &edfs, &select_time_out);

        if (res == -1) {
            break;
        }
        auto conn = m_connections.begin();
        while (conn != m_connections.end()) {
            if (FD_ISSET(conn->first, &rdfs) > 0) {
                HandleRead(conn->first);
            } else if (FD_ISSET(conn->first, &wdfs)) {
                HandleWrite(conn->first);
            }

            if (conn->second.state == CS_CLOSED) {
                auto temp = conn;
                ++conn;
                m_connections.erase(temp);
            } else {
                ++conn;
            }
        }

        if (FD_ISSET(m_l_sock, &rdfs) > 0) {
            HandleNewConnection();
        }
    }

    return true;
}

