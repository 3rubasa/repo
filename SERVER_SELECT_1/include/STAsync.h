#ifndef STASYNC_H
#define STASYNC_H

#include <map>
#include "../settings.h"

class STAsync
{
public:
    bool Init();
    bool Start();

private:
    enum ConnectionState {
        CS_UNKNOWN = 0,
        CS_READ_PENDING,
        CS_WRITE_PENDING,
        CS_CLOSED
    };

    struct ConnectionDescriptor {
        ConnectionState state;
        char in_buf[INPUT_BUFFER_SIZE_BYTES];
        char out_buf[OUTPUT_BUFFER_SIZE_BYTES];
        int out_size_bytes;
    };

private:
    void HandleNewConnection();
    void HandleRead(int sock);
    void HandleWrite(int sock);

private:
    int m_l_sock; // Listen socket.
    std::map<int, ConnectionDescriptor> m_connections;
};

#endif // STASYNC_H
