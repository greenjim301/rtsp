#include "r_thread.h"
#include <stdio.h>
#include <handleapi.h>
#include <windef.h>
#include <WinBase.h>
#include "r_util.h"

unsigned  WINAPI ThreadFunction( LPVOID lpParam )
{
    r_thread* ctx = static_cast<r_thread*>(lpParam);
    int ret = ctx->Run();
    _endthreadex( 0 );
    return ret;
}

r_thread::r_thread()
: m_handle(NULL)
, m_mutex(NULL)
, m_exit_flag(0)
{
}

r_thread::~r_thread()
{
    if (m_handle != NULL) {
        CloseHandle(m_handle);
    }

    if (m_mutex != NULL) {
        CloseHandle(m_mutex);
    }
}


int r_thread::Start()
{
    m_handle =(HANDLE) _beginthreadex(NULL, 0, ThreadFunction, this, 0, NULL);

    if (m_handle == NULL) {
        r_log("fail to create thread\n");
        return -1;
    }

    m_mutex = CreateMutex(NULL, FALSE, NULL);
    return 0;
}

void r_thread::RLock()
{
    WaitForSingleObject(m_mutex, INFINITE);
}


void r_thread::RUnlock()
{
    ReleaseMutex(m_mutex);
}

int r_thread::WaitStop()
{
    WaitForSingleObject(m_handle, 5000);
    CloseHandle( m_handle );  
    m_handle = NULL;

    CloseHandle( m_mutex );
    m_mutex = NULL;
    return 0;
}

