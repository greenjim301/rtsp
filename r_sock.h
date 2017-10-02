#ifndef __R_SOCK_H__
#define __R_SOCK_H__

#ifdef _WIN32
#include <WinSock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif // _WIN32

#include "r_string.h"

class r_sock
{
public:
    r_sock();
    ~r_sock();

    int Connect(r_string& host, int port);
    int Send(const char* buf, int size);
    int Recv(char* buf, int size);

private:
    void Close();
    bool Valid();

private:
#ifdef _WIN32
    SOCKET m_sock;
#else
    int m_sock;
#endif
};

#endif
