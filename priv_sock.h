#ifndef _PRIV_SOCK_H_
#define _PRIV_SOCK_H_

#include "session.h"
#define PRIV_SOCK_OPERATION_SUCCEEDED 0
#define PRIV_SOCK_OPERATION_FAILED -1

#define PRIV_SOCK_EOF 0xff
#define PRIV_SOCK_INVALID_COMMAND 0
#define PRIV_SOCK_INVALID_RESULT 0

#define PRIV_SOCK_GET_DATA_SOCK 1
#define PRIV_SOCK_PASV_ACTIVE 2
#define PRIV_SOCK_PASV_LISTEN 3
#define PRIV_SOCK_PASV_ACCEPT 4

#define PRIV_SOCK_RESULT_OK 1
#define PRIV_SOCK_RESULT_BAD 2

void priv_sock_init(Session_t *session);
void priv_sock_close(Session_t *session);
void priv_sock_set_nobody_context(Session_t *session);
void priv_sock_set_proto_context(Session_t *session);
int priv_sock_send_cmd(SOCKET fd, char cmd);
int priv_sock_recv_cmd(SOCKET fd, char* pcmd);
int priv_sock_send_result(SOCKET fd, char res);
int priv_sock_recv_result(SOCKET fd, char* pres);
int priv_sock_send_int(SOCKET fd, int res);
int priv_sock_recv_int(SOCKET fd, int* pres);
int priv_sock_send_str(SOCKET fd, const char *buf, unsigned int len);
int priv_sock_recv_str(SOCKET fd, char *buf, unsigned int len);
int priv_sock_send_fd(SOCKET sock_fd, SOCKET fd);
int priv_sock_recv_fd(SOCKET sock_fd, SOCKET* pfd);

#endif 
