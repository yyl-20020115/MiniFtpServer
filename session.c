#include "session.h"
#include "common.h"
#include "ftp_proto.h"
#include "ftp_nobody.h"
#include "priv_sock.h"
#include "configure.h"
#include "session_manager.h"
void session_free(Session_t* session)
{
    if (session != 0) { 
        remove_session(session);
        s_close(&session->peer_fd);
        s_close(&session->nobody_fd);
        s_close(&session->proto_fd);
        s_close(&session->data_fd);
        s_close(&session->listen_fd);
        if (session->p_addr != 0) {
            free(session->p_addr);
            session->p_addr = 0;
        }
        memset(session, 0, sizeof(Session_t));
        free(session);
    }
}
void session_init(Session_t *session)
{
    if (session != 0) {
        memset(session, 0, sizeof(Session_t));

        session->peer_fd = -1;
        session->nobody_fd = -1;
        session->proto_fd = -1;
        session->data_fd = -1;
        session->listen_fd = -1;

        session->user_uid = 0;
        session->ascii_mode = 0;

        session->p_addr = NULL;

        session->restart_pos = 0;
        session->rnfr_name = NULL;

        session->limits_max_upload = tunable_upload_max_rate * 1024;
        session->limits_max_download = tunable_download_max_rate * 1024;
        session->start_time_sec = 0;
        session->start_time_usec = 0;

        session->is_translating_data = 0;
        session->is_receive_abor = 0;

        session->curr_clients = 0;
        session->curr_ip_clients = 0;

        add_session(session);
    }
}

void session_reset_command(Session_t *session)
{
    if (session != 0) 
    {
        memset(session->command, 0, sizeof(session->command));
        memset(session->com, 0, sizeof(session->com));
        memset(session->args, 0, sizeof(session->args));
    }
}
#ifndef _WIN32
static int private_thread(void* lp)
#else
static DWORD WINAPI private_thread(void* lp) 
#endif
{
    int r = 0;
    Session_t* session = (Session_t*)lp;
    if (session != 0) {
        r = handle_proto(session);
    }
    return r;
}
int start_private(Session_t* session, void** ph)
{
    int tid = -1;
    *ph = CreateThread(NULL, 0, private_thread, session, 0, (DWORD*)&tid);    
    return tid;
}
