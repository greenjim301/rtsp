#include "rtsp.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "r_util.h"

const char kD3DClassName[] = "d3d_renderer";

static rtsp_ctx* g_ctx = NULL;

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
	else if (msg == WM_SIZE)
	{
		if (g_ctx)
		{
			RECT rect;
			GetClientRect(hwnd, &rect);
			g_ctx->m_rev_thread.m_vren_thread.TryResizeViewport(rect.right, rect.bottom);
		}

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

	g_ctx = new rtsp_ctx;
	r_string url("rtsp://192.168.2.31:554/cam/realmonitor?channel=1&subtype=0&unicast=true&proto=Onvif");

    WNDCLASSA wc = {};

    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = rtsp_ctx::WindowProc;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW);
    wc.lpszClassName = kD3DClassName;

    RegisterClassA(&wc);

	g_ctx->m_hwhd = CreateWindowA(kD3DClassName,
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

    if (g_ctx->m_hwhd == NULL) {
        return Cleanup(g_ctx, -1);
    }

    // 显示窗口  
    ShowWindow(g_ctx->m_hwhd, SW_SHOW);

    // 更新窗口  
    UpdateWindow(g_ctx->m_hwhd);
	
	g_ctx->m_rev_thread.SetParam(g_ctx->m_hwhd, url);
	g_ctx->m_rev_thread.Start();
    
    MSG msg;  
    while(GetMessage(&msg, NULL, 0, 0))  
    {  
        TranslateMessage(&msg);  
        DispatchMessage(&msg);  
    }  

	g_ctx->m_rev_thread.SetExitFlag();
	g_ctx->m_rev_thread.WaitStop();

    r_log("end\n");
    return Cleanup(g_ctx, 0);
}
