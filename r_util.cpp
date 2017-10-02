#include "r_util.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <windef.h>
#include <winuser.h>
#include <stdarg.h>
#include <stringapiset.h>
#include <string>

void SkipSpace(char*& p)
{
    while(*p == ' ')
        ++p;
}

int SkipLine(char*& p)
{
    char* pp = strstr(p, "\r\n");
    if(pp)
    {
        p = pp + strlen("\r\n");
        return 0;
    }

    return -1;	
}

int get_rtsp_addr(r_string& url, r_string&  host, int& port)
{
    port = 554;

    if(0 != memcmp(url.c_str(), "rtsp://", strlen("rtsp://")))
    {
        r_log("%s illegal\n", url.c_str());
        return -1;
    }

    char* start = url.Data() + strlen("rtsp://");
    char* p = strchr(start, ':');
    if(p)
    {
        host.assign(start, p - start);
        ++p;
        port = atoi(p);
    }
    else
    {
        p = strchr(start, '/');
        if(p)
        {
            host.assign(start, p - start);
        }
        else
        {
            host.assign(start, url.Size() - (start - url.c_str()));
        }
    }

    r_log("host:%s, port:%d\n", host.c_str(), port);
    return 0;
}

void r_log(const char* pszFormat, ...)  
{  
	va_list pArgs;  

	char szMessageBuffer[64]={0};  
	va_start( pArgs, pszFormat );  
	_vsnprintf( szMessageBuffer, 64, pszFormat, pArgs );  
	va_end( pArgs );  

	TCHAR buf[64] = {0};

#ifdef UNICODE  
	MultiByteToWideChar(CP_ACP, 0, szMessageBuffer, -1, buf, 64);  
#else  
	strcpy(buf, strszMessageBufferUsr);  
#endif 

	OutputDebugString(buf);  

}  
