#include "vren_thread.h"

extern "C"
{
#include "libavformat/avformat.h"
};

#include "libyuv.h"
#include "r_util.h"
#include <d3d11.h>


static  IDXGISwapChain* g_pSwapChain = NULL;
static  ID3D11Device* g_pd3dDevice = NULL;
static  ID3D11DeviceContext* g_pImmediateContext = NULL;
static ID3D11RenderTargetView* g_pRenderTargetView = NULL;
static ID3D11Texture2D * g_Texture = NULL;


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
	, m_d3dinit(false)
{
}

static double  gT1 = 0;
static int  gCount = 0;
static double t1;
static double t0;
static double ta;

int vren_thread::Render(AVFrame* frame)
{
	t1 = GetTickCount();

	if (t0)
	{
		gT1 += t1 - t0;
	}
	else {
		ta = t1;
	}

	int ret = this->Render_d3d(frame);

	t0 = GetTickCount();

	++gCount;

	if (gCount == 1000)
	{
		r_log("all time:%.4f, render time:%.4f\n",  t0 - ta, t0 - ta - gT1);

		gCount = 0;
		gT1 = 0;
		t0 = 0;
	}	

	return ret;
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
    if(!m_d3dinit)
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


	ID3D11Texture2D* pBackBuffer;
	// Get a pointer to the back buffer
	g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
		(LPVOID*)&pBackBuffer);

	g_pImmediateContext->UpdateSubresource(pBackBuffer, 0, NULL, m_y_data, stride * 4, 0);

	g_pSwapChain->Present(0, 0);

	return 0;

    /*D3DLOCKED_RECT d3d_rect;  
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
	*/
    return 0;
}

int vren_thread::InitRender_d3d(size_t width, size_t height)
{
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.Format =  DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = m_hwhd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	HRESULT hr = S_OK;
	D3D_FEATURE_LEVEL  FeatureLevelsRequested = D3D_FEATURE_LEVEL_11_0;
	UINT               numLevelsRequested = 1;
	D3D_FEATURE_LEVEL  FeatureLevelsSupported;

	if (FAILED(hr = D3D11CreateDeviceAndSwapChain(NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		0,
		&FeatureLevelsRequested,
		numLevelsRequested,
		D3D11_SDK_VERSION,
		&sd,
		&g_pSwapChain,
		&g_pd3dDevice,
		&FeatureLevelsSupported,
		&g_pImmediateContext)))
	{
		return hr;
	}

	ID3D11Texture2D* pBackBuffer;
	// Get a pointer to the back buffer
	hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
		(LPVOID*)&pBackBuffer);

	// Create a render-target view
	g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL,
		&g_pRenderTargetView);

	// Bind the view
	g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, NULL);

	// Setup the viewport
	D3D11_VIEWPORT vp;
	vp.Width = width;
	vp.Height = height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	g_pImmediateContext->RSSetViewports(1, &vp);

	m_width = width;
	m_height = height;

    SetWindowPos(m_hwhd, NULL, 0, 0, width, height, SWP_NOZORDER);

	m_d3dinit = true;
	m_y_data = new uint8_t[width*height * 4];

	return 0;

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

	
