#ifndef __VREN_THREAD_H__
#define __VREN_THREAD_H__

#include "ren_thread.h"

class vren_thread : public ren_thread
{
public:
    vren_thread();

    inline void SetParam(HWND& hwnd)
    {
        m_hwhd = hwnd;
    }

private:
    int Render(AVFrame* frame);
    void DestroyRender();
    int InitRender(size_t width, size_t height);

private:
    HWND m_hwhd;
    IDirect3D9* m_d3d;
    IDirect3DDevice9* m_d3d_device;
    IDirect3DSurface9* m_surface;
    RECT m_rtViewport; 
	int m_width;
	int m_height;
};

#endif
