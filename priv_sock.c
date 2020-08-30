#include "priv_sock.h"
#include "common.h"
#include "sysutil.h"
#include "trans_ctrl.h"
#ifdef _WIN32
static int __stream_socketpair(struct addrinfo* addr_info, SOCKET sock[2]) {
    SOCKET listener, client, server;
    int opt = 1;

    listener = server = client = INVALID_SOCKET;
    listener = socket(addr_info->ai_family, addr_info->ai_socktype, addr_info->ai_protocol); 
    
    if (INVALID_SOCKET == listener)
        goto fail;

    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    if (SOCKET_ERROR == bind(listener, addr_info->ai_addr,(int) addr_info->ai_addrlen))
        goto fail;

    if (SOCKET_ERROR == getsockname(listener, addr_info->ai_addr, (int*)&addr_info->ai_addrlen))
        goto fail;

    if (SOCKET_ERROR == listen(listener, 5))
        goto fail;

    client = socket(addr_info->ai_family, addr_info->ai_socktype, addr_info->ai_protocol); 
    
    if (INVALID_SOCKET == client)
        goto fail;

    if (SOCKET_ERROR == connect(client, addr_info->ai_addr,(int) addr_info->ai_addrlen))
        goto fail;

    server = accept(listener, 0, 0);

    if (INVALID_SOCKET == server)
        goto fail;

    s_close(&listener);

    sock[0] = client;
    sock[1] = server;

    return 0;
fail:
    s_close(&listener);
    s_close(&client);
    return -1;
}

int socketpair(int family, int type, int protocol, SOCKET recv[2]) {
    const char* address;
    struct addrinfo addr_info, * p_addrinfo;
    int result = -1;
    
    memset(&addr_info, 0, sizeof(addr_info));
    addr_info.ai_family = family;
    addr_info.ai_flags = AI_PASSIVE;
    addr_info.ai_socktype = type;
    addr_info.ai_protocol = protocol;
    if (AF_INET6 == family)
    {
        address = "0:0:0:0:0:0:0:1";
    }
    else 
    {
        address = "127.0.0.1";
    }
    if (0 == getaddrinfo(address, 0, &addr_info, &p_addrinfo)) 
    {
        if (SOCK_STREAM == type)
            result = __stream_socketpair(p_addrinfo, recv);   //use for tcp
        freeaddrinfo(p_addrinfo);
    }
    return result;
}
#endif

void priv_sock_init(Session_t *session)
{
    SOCKET fds[2] = { 0 };
    if (socketpair(
#ifndef _WIN32
        PF_UNIX
#else
        AF_INET
#endif
        , SOCK_STREAM, 0, fds) == -1) {
        exit_with_error("socketpair failed");
        return;
    }
    session->nobody_fd = fds[0];
    session->proto_fd = fds[1];
}
void priv_sock_close(Session_t *session)
{
    s_close(&session->nobody_fd);
    s_close(&session->proto_fd);
}
int priv_sock_send_cmd(SOCKET fd, char cmd)
{
    int ret = writen(fd, &cmd, sizeof(cmd));
    if(ret != sizeof(cmd))
    {
        exit_with_error("priv_sock_send_cmd error\n");
        return PRIV_SOCK_OPERATION_FAILED;
    }
    else {
        return PRIV_SOCK_OPERATION_SUCCEEDED;
    }
}
int priv_sock_recv_cmd(SOCKET fd, char* pres)
{
    int ret = readn(fd, pres, sizeof(char));

    if(ret == 0)
    {
        *pres = PRIV_SOCK_EOF;
        exit_with_message("Proto close!\n");
        return PRIV_SOCK_OPERATION_SUCCEEDED;
    }
    else if(ret != sizeof(char))
    {
        *pres = PRIV_SOCK_INVALID_COMMAND;
        exit_with_error("priv_sock_recv_cmd error\n");
        return PRIV_SOCK_OPERATION_FAILED;
    }

    return PRIV_SOCK_OPERATION_SUCCEEDED;
}
int priv_sock_send_result(SOCKET fd, char res)
{
    int ret = writen(fd, &res, sizeof(res));
    if(ret != sizeof(res))
    {
        exit_with_error("priv_sock_send_result\n");
        return PRIV_SOCK_OPERATION_FAILED;
    }
    return PRIV_SOCK_OPERATION_SUCCEEDED;
}

int priv_sock_recv_result(SOCKET fd, char* pres)
{
    int ret = readn(fd, pres, sizeof(char));
    if(ret != sizeof(char))
    {
        *pres = PRIV_SOCK_INVALID_RESULT;
        exit_with_error("priv_sock_recv_result error\n");
        return  PRIV_SOCK_OPERATION_FAILED;
    }
    return PRIV_SOCK_OPERATION_SUCCEEDED;
}

int priv_sock_send_int(SOCKET fd, int res)
{
    int ret = writen(fd, (char*)&res, sizeof(int));
    if(ret != sizeof(int))
    {
        exit_with_error("priv_sock_send_int error\n");
        return PRIV_SOCK_OPERATION_FAILED;
    }
    return PRIV_SOCK_OPERATION_SUCCEEDED;
}

int priv_sock_recv_int(SOCKET fd, int* pres)
{
    int ret = readn(fd, pres, sizeof(int));
    if(ret != sizeof(int))
    {
        *pres = PRIV_SOCK_INVALID_RESULT;
        exit_with_error("priv_sock_recv_int error\n");
        return PRIV_SOCK_OPERATION_FAILED;
    }
    return PRIV_SOCK_OPERATION_SUCCEEDED;
}

int priv_sock_send_str(SOCKET fd, const char *buf, unsigned int len)
{
    if (PRIV_SOCK_OPERATION_SUCCEEDED
        != priv_sock_send_int(fd, (int)len))
    {
        return PRIV_SOCK_OPERATION_FAILED;
    }
    int ret = writen(fd, buf, len);
    if(ret != (int)len)
    {
        exit_with_error("priv_sock_send_str error\n");
        return PRIV_SOCK_OPERATION_FAILED;
    }
    return PRIV_SOCK_OPERATION_SUCCEEDED;
}


int priv_sock_recv_str(SOCKET fd, char *buf, unsigned int len)
{
    unsigned int recv_len = 0;

    if (PRIV_SOCK_OPERATION_SUCCEEDED
        != priv_sock_recv_int(fd, &recv_len)
        )
    {
        return PRIV_SOCK_OPERATION_FAILED;
    }
    if (recv_len > len)
    {
        exit_with_error("priv_sock_recv_str error\n");
        return PRIV_SOCK_OPERATION_FAILED;
    }

    int ret = readn(fd, buf, recv_len);
    if (ret != (int)recv_len)
    {
        exit_with_error("priv_sock_recv_str error\n");
        return PRIV_SOCK_OPERATION_FAILED;
    }
    return PRIV_SOCK_OPERATION_SUCCEEDED;
}

int priv_sock_send_fd(SOCKET sock_fd, SOCKET fd)
{
    return priv_sock_send_int(sock_fd, (int)fd);
}

int priv_sock_recv_fd(SOCKET sock_fd, SOCKET* pfd)
{
    return priv_sock_recv_int(sock_fd, (int*)pfd);
}
