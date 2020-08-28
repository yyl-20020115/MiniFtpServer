#include "common.h"
#include "sysutil.h"
#include "session.h"
#include "configure.h"
#include "parse_conf.h"
#include "ftp_assist.h"
#include "ftp_nobody.h"
#include "priv_sock.h"

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
int start_private(Session_t* l_sess);

#ifndef _WIN32
static int session_thread(void* lp)
#else
static DWORD WINAPI session_thread(void* lp) 
#endif
{
    int r = 0;
    Session_t* session = (Session_t*)lp;

    limit_num_clients(session);

    if (start_private(session) < 0) {
        r = exit_with_error("failed to start private thread");
    }
    if (r >= 0) {
        priv_sock_set_nobody_context(session);
        //r = 0 if correctly exits
        r = handle_nobody(session);
    }
    if (session != 0) {
        s_close(&session->peer_fd);
        free(session);
    }

    return r;
}
tid_t start_session(Session_t* session)
{
    tid_t tid = -1;
#ifndef _WIN32
    //PTHREAD
#else
    CreateThread(NULL, 0, session_thread, session, 0, (DWORD*)&tid);
#endif
    return tid;
}

static SOCKET listen_fd = 0;

void exit_loop() {
    s_close(&listen_fd);
}

int main_loop() 
{
    init_hash();

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

            add_clients_to_hash(session, ip);
            
            tid_t tid = start_session(session);
            if (tid>=0)
            {
                add_tid_ip_to_hash(tid, ip);
            }
            else {
                s_close(&peer_fd);
                free(session);
                exit_with_error("unable to start session!");
                break;
            }
        }
    }
    s_close(&listen_fd);
    free_hash();

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
