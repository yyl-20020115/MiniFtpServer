#include "session_manager.h"
#include "cvector.h"

#ifndef _WIN32
#else
#include <Windows.h>
static CRITICAL_SECTION cs = { 0 };
#endif

static cvector cv = 0;

int init_session_manager()
{
    cv = cvector_create(sizeof(Session_t*));
#ifndef _WIN32
#else
    InitializeCriticalSection(&cs);
#endif
    return 0;
}

int destroy_session_manager()
{
    cvector_destroy(cv);
#ifndef _WIN32
#else
    DeleteCriticalSection(&cs);
#endif
    return 0;
}

int add_session(Session_t* session)
{
#ifndef _WIN32
#else
    EnterCriticalSection(&cs);
#endif
    if(session!=0)
    {
        cvector_pushback(cv, session);
    }
#ifndef _WIN32
#else
    LeaveCriticalSection(&cs);
#endif
    return 0;
}

int remove_session(Session_t* session)
{
#ifndef _WIN32
#else
    EnterCriticalSection(&cs);
#endif
    if (session != 0)
    {
        citerator ft = 0;
        for (citerator it = cvector_begin(cv); it != cvector_end(cv); it = cvector_next(cv, it))
        {
            Session_t* s = *(Session_t**)it;
            if (s == session) {
                ft = it;
                break;
            }
        }
        if (ft != 0) {
            cvector_rm(cv, ft);
        }
    }
#ifndef _WIN32
#else
    LeaveCriticalSection(&cs);
#endif
    return 0;
}

static size_t dump_handles(void*** pphs) 
{
    *pphs = 0;
    size_t i = 0, count = 0;
#ifndef _WIN32
#else
    EnterCriticalSection(&cs);
#endif
    if ((count = cvector_length(cv)) > 0)
    {
        *pphs = (HANDLE*)malloc(sizeof(HANDLE) * count);
        if (*pphs == 0)
        {
            count = 0;
        }
        else
        {
            for (citerator it = cvector_begin(cv); it != cvector_end(cv); it = cvector_next(cv, it))
            {
                Session_t* s = *(Session_t**)it;
                if (s != 0) {
                    *pphs[i] = s->handle;
                    i++;
                }
            }
        }
    }
#ifndef _WIN32
#else
    LeaveCriticalSection(&cs);
#endif
    return count;
}
int wait_sessions()
{
    DWORD r = 0;
    void** phs = 0;
    size_t count = dump_handles(&phs);
    if (count > 0) {
#ifndef _WIN32
#else
        r = WaitForMultipleObjects((DWORD)count, phs, TRUE, INFINITE);
#endif
        free(phs);
    }
    return 0;
}
