#include "r_sock.h"
#include <stdio.h>
#include <errno.h>
#include "r_util.h"

#ifdef _LINUX
#include <unistd.h>
#endif

r_sock::r_sock()
#ifdef _WIN32
    : m_sock(INVALID_SOCKET)
#else
    : m_sock(-1)
#endif
{
}

r_sock::~r_sock()
{
    this->Close();
}

void r_sock::Close()
{
    if(this->Valid())
    {
#ifdef _WIN32
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
#else
        close(m_sock);
        m_sock = -1;
#endif
    }
}

bool r_sock::Valid()
{
#ifdef _WIN32
    return m_sock != INVALID_SOCKET;
#else
    return m_sock != -1;
#endif // _WIN32 
}

int r_sock::Connect(r_string& host, int port )
{
    m_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(!this->Valid())
    {
        r_log("fail to create sock, err:%d\n", errno);
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(host.c_str());
    server_addr.sin_port = htons(port);

    int ret = connect(m_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if(ret)
    {
        r_log("fail to connect %s:%d, err:%d\n", host.c_str(), port, errno);
        this->Close();
        return -1;
    }

    return 0;
}

int r_sock::Send(const char* buf, int size)
{
    return send(m_sock, buf, size, 0);
}

int r_sock::Recv(char* buf, int size)
{
    return recv(m_sock, buf, size, 0);
}
