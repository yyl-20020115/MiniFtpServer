#include "priv_command.h"
#include "common.h"
#include "sysutil.h"
#include "priv_sock.h"
#include "configure.h"

int privop_pasv_get_data_sock(Session_t* session)
{
    char ip[16] = { 0 };

    if (PRIV_SOCK_OPERATION_SUCCEEDED
        != priv_sock_recv_str(session->nobody_fd, ip, sizeof(ip)))
        return PRIV_SOCK_OPERATION_FAILED;

    uint16_t port = 0;
    int res = 0;
    if (PRIV_SOCK_OPERATION_SUCCEEDED
        != priv_sock_recv_int(session->nobody_fd, &res))
        return PRIV_SOCK_OPERATION_FAILED;

    port = (uint16_t)res;
    SOCKET data_fd = tcp_client(20);
    struct sockaddr_in addr = { 0 };

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_port = htons(port);

    int ret = connect_timeout(data_fd, &addr, tunable_connect_timeout);
    if (ret == -1) {
        exit_with_error("connect_timeout");
        return PRIV_SOCK_OPERATION_FAILED;
    }
    if (PRIV_SOCK_OPERATION_SUCCEEDED
        != priv_sock_send_result(
            session->nobody_fd,
            PRIV_SOCK_RESULT_OK))
        return PRIV_SOCK_OPERATION_FAILED;
    if (PRIV_SOCK_OPERATION_SUCCEEDED
        != priv_sock_send_fd(
            session->nobody_fd,
            data_fd))
        return PRIV_SOCK_OPERATION_FAILED;
    return PRIV_SOCK_OPERATION_SUCCEEDED;
}

int privop_pasv_active(Session_t *session)
{
    return priv_sock_send_int(session->nobody_fd, (session->listen_fd != -1));
}

int privop_pasv_listen(Session_t *session)
{
    char ip[16] = {0};
    get_local_ip(ip);
    SOCKET listenfd = tcp_server(ip, 20);
    session->listen_fd = listenfd;

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (getsockname(listenfd, (struct sockaddr*)&addr, &len) == -1)
    {
        exit_with_error("getsockname");
        return PRIV_SOCK_OPERATION_FAILED;
    }
    if (PRIV_SOCK_OPERATION_SUCCEEDED
        != priv_sock_send_result(session->nobody_fd, PRIV_SOCK_RESULT_OK))
        return PRIV_SOCK_OPERATION_FAILED;
    
    uint16_t net_endian_port = ntohs(addr.sin_port);
    if (PRIV_SOCK_OPERATION_SUCCEEDED
        != priv_sock_send_int(session->nobody_fd, net_endian_port))
        return PRIV_SOCK_OPERATION_FAILED;

    return PRIV_SOCK_OPERATION_SUCCEEDED;
}

int privop_pasv_accept(Session_t *session)
{
    SOCKET peer_fd = accept_timeout(session->listen_fd, NULL, tunable_accept_timeout);

    s_close(&session->listen_fd);

    if(peer_fd == -1)
    {
        if (PRIV_SOCK_OPERATION_SUCCEEDED
            != priv_sock_send_result(session->nobody_fd, PRIV_SOCK_RESULT_BAD))
            return PRIV_SOCK_OPERATION_FAILED;

        
        exit_with_error("accept_timeout");
        return PRIV_SOCK_OPERATION_FAILED;
    }

    if (PRIV_SOCK_OPERATION_SUCCEEDED
        != priv_sock_send_result(session->nobody_fd, PRIV_SOCK_RESULT_OK))
        return PRIV_SOCK_OPERATION_FAILED;

    if (PRIV_SOCK_OPERATION_SUCCEEDED
        != priv_sock_send_fd(session->nobody_fd, peer_fd))
        return PRIV_SOCK_OPERATION_FAILED;

    return PRIV_SOCK_OPERATION_SUCCEEDED;
}
