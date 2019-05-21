#include "vren_thread.h"

extern "C"
{
#include "libavformat/avformat.h"
};

#include "libyuv.h"

vren_thread::vren_thread()
    : m_hwhd(NULL)
    , m_d3d(NULL)
    , m_d3d_device(NULL)
    , m_surface(NULL)
	, m_glInit(false)
	, m_ghDC(0)
	, m_ghRC(0)
	, m_y_data(NULL)
	, m_try_resize(false)
{
}


int vren_thread::Render(AVFrame* frame)
{
	return this->Render_gl(frame);
}

void vren_thread::DestroyRender()
{
	if (m_glInit)
	{
		this->DestroyRender_gl();
	}
	else
	{
		this->DestroyRender_d3d();
	}
}

int vren_thread::Render_d3d(AVFrame* frame)
{
    if(m_d3d == NULL)
    {
        if(this->InitRender_d3d(frame->width, frame->height))
            return -1;

    }

	if (m_width != frame->width || m_height != frame->height)
	{
		this->DestroyRender_d3d();

		if (this->InitRender_d3d(frame->width, frame->height))
			return -1;
	}

    D3DLOCKED_RECT d3d_rect;  
    if(D3D_OK !=  m_surface->LockRect(&d3d_rect,NULL,D3DLOCK_DONOTWAIT))
    {
        return -1;
    }

    uint8_t * pDest = (BYTE *)d3d_rect.pBits;  
    size_t width = frame->width;
    size_t height = frame->height;
	int stride =  d3d_rect.Pitch;

	for (size_t i = 0; i < height; i++) {
		memcpy(pDest + i * stride, frame->data[0] + i * stride, stride);
	}
	for (size_t i = 0; i < height / 2; i++) {
		memcpy(pDest + stride * height + i * stride / 2, frame->data[2] + i * stride / 2, stride / 2);
	}
	for (size_t i = 0; i < height / 2; i++) {
		memcpy(pDest + stride * height + stride * height / 4 + i * stride / 2, frame->data[1] + i * stride / 2, stride / 2);
	}

    if(D3D_OK !=  m_surface->UnlockRect())
    {
        return -1;
    }

    m_d3d_device->Clear( 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0,0,0), 1.0f, 0 );  
    m_d3d_device->BeginScene();  
    IDirect3DSurface9 * pBackBuffer = NULL;  

    m_d3d_device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&pBackBuffer);  
    m_d3d_device->StretchRect(m_surface,NULL,pBackBuffer,&m_rtViewport,D3DTEXF_LINEAR);  
    m_d3d_device->EndScene();  
    m_d3d_device->Present( NULL, NULL, NULL, NULL );  
    pBackBuffer->Release();  

    return 0;
}

int vren_thread::InitRender_d3d(size_t width, size_t height)
{
	m_width = width;
	m_height = height;

    SetWindowPos(m_hwhd, NULL, 0, 0, width, height, SWP_NOZORDER);

    m_d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (m_d3d == NULL) {
        this->DestroyRender_d3d();
        return -1;
    }

    GetClientRect(m_hwhd,&m_rtViewport);  

    D3DPRESENT_PARAMETERS d3d_params = {0};

    d3d_params.Windowed = TRUE;  
    d3d_params.SwapEffect = D3DSWAPEFFECT_DISCARD;  
    d3d_params.BackBufferFormat = D3DFMT_UNKNOWN; 

    if (m_d3d->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,m_hwhd,  
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,  
        &d3d_params, &m_d3d_device ) != D3D_OK) {
            this->DestroyRender_d3d();
            return -1;
    }

    if(m_d3d_device->CreateOffscreenPlainSurface(  
        width,height,  
        (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2'),  
        D3DPOOL_DEFAULT,  
        &m_surface,  
        NULL) != D3D_OK)
    {
        this->DestroyRender_d3d();
        return -1;
    }

    return 0;
}

void vren_thread::DestroyRender_d3d()
{
    if(m_surface)  
        m_surface->Release();  
    if(m_d3d_device)  
        m_d3d_device->Release();  
    if(m_d3d)  
        m_d3d->Release();

    m_surface = NULL;
    m_d3d_device = NULL;
    m_d3d = NULL;
}

BOOL bSetupPixelFormat(HDC hdc)
{
	PIXELFORMATDESCRIPTOR pfd, *ppfd;
	int pixelformat;

	ppfd = &pfd;

	ppfd->nSize = sizeof(PIXELFORMATDESCRIPTOR);
	ppfd->nVersion = 1;
	ppfd->dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL |
		PFD_DOUBLEBUFFER;
	ppfd->dwLayerMask = PFD_MAIN_PLANE;
	ppfd->iPixelType = PFD_TYPE_RGBA;
	ppfd->cColorBits = 24;
	ppfd->cDepthBits = 16;
	ppfd->cAccumBits = 0;
	ppfd->cStencilBits = 0;

	pixelformat = ChoosePixelFormat(hdc, ppfd);

	if ((pixelformat = ChoosePixelFormat(hdc, ppfd)) == 0)
	{
		//MessageBox(NULL, "ChoosePixelFormat failed", "Error", MB_OK);
		return FALSE;
	}

	if (SetPixelFormat(hdc, pixelformat, ppfd) == FALSE)
	{
		//MessageBox(NULL, "SetPixelFormat failed", "Error", MB_OK);
		return FALSE;
	}

	return TRUE;
}

//GLvoid initializeGL(GLsizei width, GLsizei height)
int vren_thread::InitRender_gl(size_t width, size_t height)
{
	m_width = width;
	m_height = height;

	SetWindowPos(m_hwhd, NULL, 0, 0, width, height, SWP_NOZORDER);
	GetClientRect(m_hwhd, &m_rtViewport);

	m_ghDC = GetDC(m_hwhd);

	if (!bSetupPixelFormat(m_ghDC))
		return -1;

	m_ghRC = wglCreateContext(m_ghDC);
	wglMakeCurrent(m_ghDC, m_ghRC);

	glViewport(0, 0, width, height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glOrtho(0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 1.0f);
	glMatrixMode(GL_MODELVIEW);

	m_y_data = new uint8_t[width*height * 4];

	glGenTextures(1, &m_y_texture);

	glBindTexture(GL_TEXTURE_2D, m_y_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_y_data);

	m_glInit = true;

	return 0;
}

void vren_thread::TryResizeViewport(size_t width, size_t height) {

	if (m_glInit)
	{
		m_new_height = height;
		m_new_width = width;
		m_try_resize = true;
	}
}

void vren_thread::ResizeViewport(size_t width, size_t height) {
	// TODO(pbos): Aspect ratio, letterbox the video.

	m_try_resize = false;

	glViewport(0, 0, width, height);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glOrtho(0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 1.0f);
	glMatrixMode(GL_MODELVIEW);
}

int vren_thread::Render_gl(AVFrame* frame)
{
	if (!m_glInit)
	{
		if (this->InitRender_gl(frame->width, frame->height))
			return -1;
	}

	if (m_width != frame->width || m_height != frame->height)
	{
		this->DestroyRender_gl();

		if (this->InitRender_gl(frame->width, frame->height))
			return -1;
	}

	if (m_try_resize)
	{
		this->ResizeViewport(m_new_width, m_new_height);
	}

	//uint8_t * pDest = m_y_data;
	size_t width = frame->width;
	size_t height = frame->height;
	int stride = frame->linesize[0]; //d3d_rect.Pitch;
	
	libyuv::I420ToARGB(frame->data[0],
			stride,
			frame->data[2],
			stride / 2,
			frame->data[1],
			stride / 2,
			m_y_data,
			stride * 4,
			width,
			height);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, m_y_texture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, m_y_data);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glLoadIdentity();

	glBegin(GL_QUADS);
	{
		glTexCoord2f(0.0f, 0.0f);
		glVertex3f(0.0f, 0.0f, 0.0f);

		glTexCoord2f(0.0f, 1.0f);
		glVertex3f(0.0f, 1.0f, 0.0f);

		glTexCoord2f(1.0f, 1.0f);
		glVertex3f(1.0f, 1.0f, 0.0f);

		glTexCoord2f(1.0f, 0.0f);
		glVertex3f(1.0f, 0.0f, 0.0f);
	}
	glEnd();

	SwapBuffers(m_ghDC);

	return 0;
}

void vren_thread::DestroyRender_gl()
{
	m_glInit = false;

	glDeleteTextures(1, &m_y_texture);
	delete[] m_y_data;
	m_y_data = NULL;

	if (m_ghRC)
		wglDeleteContext(m_ghRC);
	if (m_ghDC)
		ReleaseDC(m_hwhd, m_ghDC);

	m_ghRC = 0;
	m_ghDC = 0;
}

	
