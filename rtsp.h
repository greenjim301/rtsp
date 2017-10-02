#ifndef __RTSP_H__
#define __RTSP_H__

#include "r_string.h"
#include "rev_thread.h"
#include <stdint.h>

class rtsp_ctx
{
public:
    rtsp_ctx();
    ~rtsp_ctx();

    static LRESULT WINAPI WindowProc(HWND hwnd, UINT msg, WPARAM wparam,
        LPARAM lparam);
   
    rev_thread m_rev_thread;
    HWND m_hwhd;
};

#endif
