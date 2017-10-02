#ifndef __R_STRING_H__
#define __R_STRING_H__

#include <string.h>

class r_string
{
public:
    r_string()
        :m_size(0)
    {
        m_buf[0] = '\0';
    }

    r_string(const char* str, const int in_size)
        : m_size(in_size)
    {
        memcpy(m_buf, str, in_size);
        m_buf[in_size] = '\0';
    }

	r_string(const char* str)
	{
		m_size = strlen(str);
		memcpy(m_buf, str, m_size);
		m_buf[m_size] = '\0';
	}
    
    r_string& operator=(const r_string& r)
    {
        m_size = r.m_size;
        memcpy(m_buf, r.m_buf, m_size);
        m_buf[m_size] = '\0';
        return *this;
    }

    void assign(const char* str, const int in_size)
    {
        memcpy(m_buf, str, in_size);
        m_size = in_size;
        m_buf[m_size] = '\0';
    }
    
    void append(const char* str, const int in_size)
    {
        memcpy(m_buf + m_size, str, in_size);
        m_size += in_size;
        m_buf[m_size] = '\0';
    }

    const char* c_str()
    {
        return m_buf;
    }

    char* Data()
    {
        return m_buf;
    }

    int Size()
    {
        return m_size;
    }

private:
    char m_buf[256];
    int m_size;
};

#endif
