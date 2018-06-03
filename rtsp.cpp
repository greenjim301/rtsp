#include "rtsp.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "r_util.h"

const char kD3DClassName[] = "d3d_renderer";

rtsp_ctx::rtsp_ctx()
{
}

rtsp_ctx::~rtsp_ctx()
{
}

LRESULT WINAPI rtsp_ctx::WindowProc(HWND hwnd, UINT msg, WPARAM wparam,  LPARAM lparam) 
{
    if (msg == WM_DESTROY || (msg == WM_CHAR && wparam == VK_RETURN)) {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

int Cleanup(rtsp_ctx* ctx, int ret)
{
    if (ctx->m_hwhd != NULL) {
        DestroyWindow(ctx->m_hwhd);
        ctx->m_hwhd = NULL;
    }

    if(ctx)
    {
        delete ctx;
    }

#ifdef _WIN32
    WSACleanup();
#endif

    return ret;
}

int CALLBACK WinMain(  
    _In_  HINSTANCE hInstance,  
    _In_  HINSTANCE hPrevInstance,  
    _In_  LPSTR lpCmdLine,  
    _In_  int nCmdShow  
    ) 
{
#ifdef _WIN32
     WSADATA wsaData = {0};
     WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

	rtsp_ctx* ctx = new rtsp_ctx;
	r_string url("rtsp://192.168.1.101:5544/mobile/test123");

    WNDCLASSA wc = {};

    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = rtsp_ctx::WindowProc;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW);
    wc.lpszClassName = kD3DClassName;

    RegisterClassA(&wc);

    ctx->m_hwhd = CreateWindowA(kD3DClassName,
        url.c_str(),
        WS_OVERLAPPEDWINDOW,
        0,
        0,
        500,
        500,
        NULL,
        NULL,
        NULL,
        NULL);

    if (ctx->m_hwhd == NULL) {
        return Cleanup(ctx, -1);
    }

    // 显示窗口  
    ShowWindow(ctx->m_hwhd, SW_SHOW);  

    // 更新窗口  
    UpdateWindow(ctx->m_hwhd);  
	
	ctx->m_rev_thread.SetParam(ctx->m_hwhd, url);	
	ctx->m_rev_thread.Start();
    
    MSG msg;  
    while(GetMessage(&msg, NULL, 0, 0))  
    {  
        TranslateMessage(&msg);  
        DispatchMessage(&msg);  
    }  

    ctx->m_rev_thread.SetExitFlag();
	ctx->m_rev_thread.WaitStop();

    r_log("end\n");
    return Cleanup(ctx, 0);
}
