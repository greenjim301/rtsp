#ifndef __VREN_THREAD_H__
#define __VREN_THREAD_H__

#include "ren_thread.h"
#include <stdint.h>
#include <GL/gl.h> 
#include <GL/glu.h> 

class vren_thread : public ren_thread
{
public:
    vren_thread();

    inline void SetParam(HWND& hwnd)
    {
        m_hwhd = hwnd;
    }

	void TryResizeViewport(size_t width, size_t height);

private:
	int Render(AVFrame* frame);
	void DestroyRender();

    int Render_d3d(AVFrame* frame);
    void DestroyRender_d3d();
    int InitRender_d3d(size_t width, size_t height);

	int InitRender_gl(size_t width, size_t height);
	int Render_gl(AVFrame* frame);
	void DestroyRender_gl();

	void ResizeViewport(size_t width, size_t height);

private:
    HWND m_hwhd;
    IDirect3D9* m_d3d;
    IDirect3DDevice9* m_d3d_device;
    IDirect3DSurface9* m_surface;
    RECT m_rtViewport; 
	int m_width;
	int m_height;
	
	bool m_glInit;
	HDC m_ghDC;
	HGLRC m_ghRC;
	uint8_t* m_y_data;
	GLuint m_y_texture;
	bool m_try_resize;
	int m_new_width;
	int m_new_height;
};

#endif
