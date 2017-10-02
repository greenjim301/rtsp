#include "vren_thread.h"

extern "C"
{
#include "libavformat/avformat.h"
};

vren_thread::vren_thread()
    : m_hwhd(NULL)
    , m_d3d(NULL)
    , m_d3d_device(NULL)
    , m_surface(NULL)
{
}

int vren_thread::Render(AVFrame* frame)
{
    if(m_d3d == NULL)
    {
        if(this->InitRender(frame->width, frame->height))
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
    int stride = d3d_rect.Pitch;  

    for(size_t i = 0;i < height;i++){  
        memcpy(pDest + i * stride, frame->data[0] + i * width, width);  
    }  
    for(size_t i = 0;i < height/2;i++){  
        memcpy(pDest + stride * height + i * stride / 2, frame->data[2]  + i * width / 2, width / 2);  
    }  
    for(size_t i = 0;i < height/2;i++){  
        memcpy(pDest + stride * height + stride * height / 4 + i * stride / 2,frame->data[1] + i * width / 2, width / 2);  
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

int vren_thread::InitRender(size_t width, size_t height)
{
    SetWindowPos(m_hwhd, NULL, 0, 0, width, height, SWP_NOZORDER);

    m_d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (m_d3d == NULL) {
        this->DestroyRender();
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
            this->DestroyRender();
            return -1;
    }

    if(m_d3d_device->CreateOffscreenPlainSurface(  
        width,height,  
        (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2'),  
        D3DPOOL_DEFAULT,  
        &m_surface,  
        NULL) != D3D_OK)
    {
        this->DestroyRender();
        return -1;
    }

    return 0;
}

void vren_thread::DestroyRender()
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
