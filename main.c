#include "common.h"
#include "sysutil.h"
#include "session.h"
#include "configure.h"
#include "parse_conf.h"
#include "ftp_nobody.h"
#include "priv_sock.h"
#include "ftp_codes.h"
void print_conf()
{
    printf("tunable_pasv_enable=%d\n", tunable_pasv_enable);
    printf("tunable_port_enable=%d\n", tunable_port_enable);

    printf("tunable_listen_port=%u\n", tunable_listen_port);
    printf("tunable_max_clients=%u\n", tunable_max_clients);
    printf("tunable_max_per_ip=%u\n", tunable_max_per_ip);
    printf("tunable_accept_timeout=%u\n", tunable_accept_timeout);
    printf("tunable_connect_timeout=%u\n", tunable_connect_timeout);
    printf("tunable_idle_session_timeout=%u\n", tunable_idle_session_timeout);
    printf("tunable_data_connection_timeout=%u\n", tunable_data_connection_timeout);
    printf("tunable_local_umask=0%o\n", tunable_local_umask);
    printf("tunable_upload_max_rate=%u\n", tunable_upload_max_rate);
    printf("tunable_download_max_rate=%u\n", tunable_download_max_rate);

    if (tunable_listen_address == NULL)
        printf("tunable_listen_address=NULL\n");
    else
        printf("tunable_listen_address=%s\n", tunable_listen_address);
}

#define H1 "There are too many connected users, please try later."
#define H2 "There are too many connections from your internet address."

void limit_num_clients(Session_t* session)
{
    if (tunable_max_clients > 0 && session->curr_clients > tunable_max_clients)
    {
        //421 There are too many connected users, please try later.
        ftp_reply(session, FTP_TOO_MANY_USERS, H1);
        exit_with_error(H1);
        return;
    }

    if (tunable_max_per_ip > 0 && session->curr_ip_clients > tunable_max_per_ip)
    {
        //421 There are too many connections from your internet address.
        ftp_reply(session, FTP_IP_LIMIT, H2);
        exit_with_error(H2);
        return;
    }
}

int s_timeout() {
#ifndef _WIN32
    return errno == ETIMEDOUT
#else
    return WSAGetLastError() == WSA_WAIT_TIMEOUT;
#endif
}
int s_close(SOCKET* s) {
    int r = 0;
    if (*s != -1) {
#ifndef _WIN32
        r = close(*s);
#else
        r = closesocket(*s);
#endif
        * s = -1;
    }
    return r;
}
static int quit = 0;
static int code = 0;
int should_exit() 
{
    return quit;
}
int exit_with_code(int c) {
    return (code = c);
}
int exit_with_error(const char* format, ...) 
{
        char buffer[1024] = { 0 };
        va_list arg;
        int done;

        va_start(arg, format);
        done = vsnprintf(buffer,sizeof(buffer), format, arg);
        va_end(arg); 
        
        fprintf(stderr, buffer);

        return exit_with_code(EXIT_FAILURE);
}
int exit_with_message(const char* format,...)
{
        va_list arg;
        int done;

        va_start(arg, format);
        done = vprintf(format, arg);
        va_end(arg);

    return exit_with_code(EXIT_SUCCESS);
}
int start_private(Session_t* l_sess, void** ph);

#ifndef _WIN32
static int session_thread(void* lp)
#else
static DWORD WINAPI session_thread(void* lp) 
#endif
{
    int r = 0;
    Session_t* session = (Session_t*)lp;

    limit_num_clients(session);
    void* h = 0;
    if (start_private(session,&h) < 0) {
        r = exit_with_error("failed to start private thread");
    }
    if (r >= 0) {
        priv_sock_set_nobody_context(session);
        //r = 0 if correctly exits
        r = handle_nobody(session);
        if (h != INVALID_HANDLE_VALUE) {
#ifndef _WIN32
            //
#else
            //waiting for private thread to quit
            WaitForSingleObject(h, INFINITE);
            CloseHandle(h);
#endif
        }
    }

    if (session != 0) {
        session_free(session);
    }

    return r;
}
tid_t start_session(Session_t* session, void** ph)
{
    tid_t tid = -1;
#ifndef _WIN32
    //PTHREAD
#else
    *ph = CreateThread(NULL, 0, session_thread, session, 0, (DWORD*)&tid);
#endif
    return tid;
}

static SOCKET listen_fd = 0;

void exit_loop() {
    s_close(&listen_fd);
}

int main_loop() 
{
    listen_fd = tcp_server(tunable_listen_address, tunable_listen_port);

    for(;;)
    {
        struct sockaddr_in addr = { 0 };
        SOCKET peer_fd = accept_timeout(listen_fd, &addr, tunable_accept_timeout);
        
        if (peer_fd == INVALID_SOCKET && s_timeout()) {
            continue;
        }
        else if (peer_fd == INVALID_SOCKET) {
            exit_with_error("accept_failed");
            break;
        }
        
        uint32_t ip = addr.sin_addr.s_addr;
        Session_t* session = (Session_t*)malloc(sizeof(Session_t));

        if (session == 0)
        {
            s_close(&peer_fd);
            exit_with_error("unable to start session!");
            break;
        }
        else
        {
            session_init(session);
            session->peer_fd = peer_fd;
            session->ip = ip;

            void* handle = 0;
            tid_t tid = start_session(session,&handle);
            if (tid>=0)
            {
                session->handle = handle;
            }
            else {
                session_free(session);
                exit_with_error("unable to start session!");
                break;
            }
        }
    }
    s_close(&listen_fd);
    return code;
}

#ifndef AS_LIBRARY
int main(int argc, const char* argv[])
{
    load_config("ftpserver.conf");
    print_conf();
    return main_loop();
}
#endif
