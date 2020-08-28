#ifndef _SESSION_H_
#define _SESSION_H_

#include "common.h"
#define MAX_COMMAND 1024

typedef struct _Session_t
{
    char command[MAX_COMMAND];
    char com[MAX_COMMAND];
    char args[MAX_COMMAND];

    uint32_t ip;
    char username[128];

    SOCKET peer_fd;
    SOCKET nobody_fd;
    SOCKET proto_fd;

    uid_t user_uid;
    int ascii_mode;

    struct sockaddr_in *p_addr;
    SOCKET data_fd;
    SOCKET listen_fd;

    long long restart_pos;
    char *rnfr_name;

    int limits_max_upload;
    int limits_max_download;
    int start_time_sec;
    int start_time_usec;

    int is_translating_data;
    int is_receive_abor;

    unsigned int curr_clients;
    unsigned int curr_ip_clients;
}Session_t;

void session_init(Session_t *session);

void session_reset_command(Session_t *session);

#endif  /*_SESSION_H_*/
