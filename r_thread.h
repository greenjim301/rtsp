#ifndef __R_THREAD_H__
#define __R_THREAD_H__

#include <process.h>
#include <synchapi.h>

class r_thread
{
public:
    r_thread();
    virtual ~r_thread();
    virtual int WaitStop();
    virtual int Run() = 0;
    int Start();
    void RLock();
    void RUnlock();
    inline void SetExitFlag(){m_exit_flag = 1;}

private:
    HANDLE m_handle;
    HANDLE m_mutex;
protected:
    int m_exit_flag;
};

#endif
