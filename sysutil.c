#include "sysutil.h"
#include "common.h"

static ssize_t recv_peek(SOCKET sockfd, void *buf, size_t len);

static int lock_file(int fd, int type);

static struct timeval tv = {0, 0}; 
#ifdef WIN32
int gettimeofday(struct timeval* tp, void* tzp)
{
    time_t clock = 0;
    struct tm tm = { 0 };
    SYSTEMTIME wtm = { 0 };
    GetLocalTime(&wtm);
    tm.tm_year = wtm.wYear - 1900;
    tm.tm_mon = wtm.wMonth - 1;
    tm.tm_mday = wtm.wDay;
    tm.tm_hour = wtm.wHour;
    tm.tm_min = wtm.wMinute;
    tm.tm_sec = wtm.wSecond;
    tm.tm_isdst = -1;
    clock = mktime(&tm);
    tp->tv_sec = (long)clock;
    tp->tv_usec = wtm.wMilliseconds * 1000;
    return (0);
}
#endif
int get_curr_time_sec()
{
    if (gettimeofday(&tv, NULL) == -1) {
        exit_with_error("gettimeofday");
    }
    return tv.tv_sec;
}

int get_curr_time_usec()
{
    return tv.tv_usec;
}
#ifndef _WIN32

int n_sleep(double t)
{
    time_t sec = (time_t)t;
    time_t nsec =(time_t)((t - sec) * 1000000000);
    //int nanosleep(const struct timespec *req, struct timespec *rem);
    struct timespec ts = { 0 };
    ts.tv_sec = (long)sec;
    ts.tv_nsec = (long)nsec;

    int ret;
    do
    {
        ret = nanosleep(&ts, &ts);
    }
    while(ret == -1 && errno == EINTR);

    return ret;
}
#endif

SOCKET tcp_client(unsigned int port)
{
    SOCKET sockfd;
    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        exit_with_error("socket");
        return sockfd;
    }

    if(port > 0)
    {
        int on = 1;
        if ((setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on))) == -1)
        {
            exit_with_error("setsockopt");
            s_close(&sockfd);
            return INVALID_SOCKET;
        }
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof addr);
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        char ip[16] = {0};
        get_local_ip(ip, sizeof(ip));
        addr.sin_addr.s_addr = inet_addr(ip);

        if (bind(sockfd, (struct sockaddr*)&addr, sizeof addr) == -1)
        {
            exit_with_error("bind");
            s_close(&sockfd);
            return INVALID_SOCKET;
        }
    }

    return sockfd;
}
int inet_aton(const char* cp, struct in_addr* inp)
{
    return inet_pton(PF_INET, cp, inp);
}
SOCKET tcp_server(const char *host, unsigned short port)
{
    int done = 0;
    SOCKET listenfd;
    if ((listenfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        exit_with_error("tcp_server");
        return INVALID_SOCKET;
    }
    struct sockaddr_in seraddr;
    memset(&seraddr, 0, sizeof(seraddr));
    seraddr.sin_family = AF_INET;
    if(host != NULL)
    {
        if(inet_aton(host, &seraddr.sin_addr) == 0)
        {
            struct hostent *hp;
            hp = gethostbyname(host);
            if (hp != 0) {
                seraddr.sin_addr = *(struct in_addr*)hp->h_addr;
                done = 1;
            }
        }
    }
    if (!done) {
        seraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    seraddr.sin_port = htons(port);

    int on = 1;
    if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on))) < 0)
    {
        exit_with_error("setsockopt");
        s_close(&listenfd);
        return INVALID_SOCKET;
    }
    if (bind(listenfd, (struct sockaddr*)&seraddr, sizeof(seraddr)) < 0)
    {
        exit_with_error("bind");
        s_close(&listenfd);
        return INVALID_SOCKET;
    }
    if (listen(listenfd, SOMAXCONN) < 0)
    {
        exit_with_error("listen");
        s_close(&listenfd);
        return INVALID_SOCKET;
    }
    return listenfd;
}
int get_local_ip(char *ip, size_t count)
{
#ifndef _WIN32
    SOCKET sockfd;
    if((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        exit_with_error("socket");
        s_close(&sockfd);
        return -1;
    }

    struct ifreq req;

    bzero(&req, sizeof(struct ifreq));
    strcpy(req.ifr_name, "eth0");

    if (ioctl(sockfd, SIOCGIFADDR, &req) == -1) {
        exit_with_error("ioctl");
        return 0;
    }
    struct sockaddr_in *host = (struct sockaddr_in*)&req.ifr_addr;
    strcpy(ip, inet_ntoa(host->sin_addr));
    s_close(&sockfd);
#else
    strncpy(ip, "0.0.0.0", count);
#endif
    return 1;
}

void activate_nonblock(SOCKET fd)
{
    u_long flags = 1;
    ioctlsocket(socket, FIONBIO, &flags);
}

void deactivate_nonblock(SOCKET fd)
{
    u_long flags = 0;
    ioctlsocket(socket, FIONBIO, &flags);
}

int read_timeout(SOCKET fd, unsigned int wait_seconds)
{
    int ret = 0;
    if(wait_seconds > 0)
    {
        fd_set read_fd = { 0 };
        FD_ZERO(&read_fd);
        FD_SET(fd, &read_fd);

        struct timeval timeout;
        timeout.tv_sec = wait_seconds;
        timeout.tv_usec = 0;

        do
        {
            ret = select(fd + 1, &read_fd, NULL, NULL, &timeout);
        }while(ret < 0 && errno == EINTR);

        if(ret == 0)
        {
            ret = -1;
            errno = ETIMEDOUT;//ETIMEDOUT--connection timed out(POSIX.1)
        }
        else if(ret == 1)
            ret = 0;
    }
    return ret;
}

int write_timeout(SOCKET fd, unsigned int wait_seconds)
{
    int ret = 0;
    if(wait_seconds > 0)
    {
        fd_set write_fd;
        FD_ZERO(&write_fd);
        FD_SET(fd, &write_fd);

        struct timeval timeout;
        timeout.tv_sec = wait_seconds;
        timeout.tv_usec = 0;

        do
        {
            ret = select(fd + 1, NULL, &write_fd, NULL,  &timeout);
        }while(ret < 0 && errno == EINTR);

        if(ret == 0)
        {
            ret = -1;
            errno = ETIMEDOUT;//ETIMEDOUT--connection timed out(POSIX.1)
        }
        else if(ret == 1)
            ret = 0;
    }
    return ret;
}

SOCKET accept_timeout(SOCKET fd, struct sockaddr_in *addr, unsigned int wait_seconds)
{
    int ret;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    if(wait_seconds > 0)
    {
        fd_set accept_fd;
        FD_ZERO(&accept_fd);
        FD_SET(fd, &accept_fd);

        struct timeval timeout;
        timeout.tv_sec = wait_seconds;
        timeout.tv_usec = 0;

        do
        {
            ret = select(accept_fd.fd_count, &accept_fd, NULL, NULL, &timeout);
        }while(ret < 0 && errno == EINTR);
        if(ret == -1)
            return -1;
        else if(ret == 0)
        {
            errno = ETIMEDOUT;
            return -1;
        }
    }
    SOCKET r;
    if(addr != NULL)
        r = accept(fd, (struct sockaddr*)addr, &addrlen);
    else
        r = accept(fd, NULL, NULL);

    return r;
}

int connect_timeout(SOCKET fd, struct sockaddr_in *addr, unsigned int wait_seconds)
{
    int ret;
    socklen_t addrlen = sizeof(struct sockaddr_in);

    if(wait_seconds > 0)
        activate_nonblock(fd);

    ret = connect(fd, (struct sockaddr*)addr, addrlen);
    printf("ret %d\n", ret);
    if(ret < 0 && errno == EINPROGRESS)
    {
        fd_set connect_fd;
        FD_ZERO(&connect_fd);
        FD_SET(fd, &connect_fd);

        struct timeval timeout;
        timeout.tv_sec = wait_seconds;
        timeout.tv_usec = 0;

        do
        {
            printf("a\n");
            /*一旦连接建立，套接字就可写*/
            ret = select(fd + 1, NULL, &connect_fd, NULL, &timeout);
            printf("ret - %d\n", ret);
        }while(ret < 0 && errno == EINTR);
        if(ret == 0)
        {
            ret = -1;
            errno = ETIMEDOUT;
        }
        else if(ret < 0)
            return -1;
        else if(ret == 1)
        {
            int err;
            socklen_t socklen = sizeof(err);
            int sockoptret = getsockopt(fd, SOL_SOCKET, SO_ERROR,(char*) &err, &socklen);
            if(sockoptret == -1)
            {
                return -1;
            }
            //else if(err == 0)
            if(err == 0)
            {
                ret = 0;
            }
            else 
            {
                errno = err;
                ret = -1;
            }
        }
    }
    if(wait_seconds > 0)
    {
        deactivate_nonblock(fd);
    }
    return ret;
}

ssize_t readn(SOCKET fd, void *buf, size_t n)
{
    size_t nleft = n;
    ssize_t nread = 0;
    char *bufp = (char*)buf;
    int e = 0;
    while(nleft > 0)
    {
#ifndef _WIN32
        if ((nread = read(fd, bufp, nleft)) < 0)
#else
        if((nread = recv(fd, bufp, nleft,0)) < 0)
#endif
        {
            if(
#ifndef _WIN32
                errno == EINTR
#else
                (e = WSAGetLastError()) == WSAEINTR
#endif
                )
                continue;
            return -1;
        }
        else if(nread == 0)
            return (n - nleft);

        bufp += nread;
        nleft -= nread;
    }
    return n;
}
ssize_t writes(SOCKET fd, const char* text) {
    return writen(fd, text, strlen(text));
}
ssize_t writen(SOCKET fd, const void *buf, size_t n)
{
    size_t nleft = n;
    ssize_t nwrite =0;
    char *bufp = (char*)buf;
    int e = 0;
    while(nleft > 0)
    {
#ifndef _WIN32
        if((nwrite = write(fd, bufp, nleft)) < 0)
#else
        if ((nwrite = send(fd, bufp, nleft, 0)) < 0)
#endif
        {
            if (
#ifndef _WIN32
                errno == EINTR
#else
                (e=WSAGetLastError()) == WSAEINTR
#endif
                )
                continue;
            return -1;
        }
        else if (nwrite == 0)
            continue;

        bufp += nwrite;
        nleft -= nwrite;
    }
    return n;
}
static ssize_t recv_peek(SOCKET sockfd, void *buf, size_t len)
{
    int nread = 0;
    for(;;)
    {
        nread = recv(sockfd, buf, len, MSG_PEEK);
        if (nread < 0 && errno == EINTR)
        {
            continue;
        }
        if(nread < 0)
        {
            return -1;
        }
        break;
    }
    return nread;
}

ssize_t readline(SOCKET sockfd, void *buf, size_t maxsize)
{
    int nread = 0;
    int nleft = 0;
    char *ptr = 0;
    int ret = 0;
    int total = 0;

    nleft = maxsize-1;
    ptr = buf;

    while (nleft > 0) {
        ret = recv_peek(sockfd, ptr, nleft);

        if(ret <= 0)
        {
            return ret;
        }

        nread = ret;
        int i = 0;
        for(i = 0; i < nread; ++i)
        {
            if (ptr[i] == '\n')
            {
                ret = readn(sockfd, ptr, i + 1);
                if (ret != i + 1)
                {
                    return -1;
                }
                total += ret;
                ptr += ret;
                *ptr = 0;
                return total;
            }
        } 
        ret = readn(sockfd, ptr, nread);
        if(ret != nread)
        {
            return -1;
        }
        nleft -= nread;
        total += nread;
        ptr += nread;
    }
    *ptr = 0;
    return maxsize - 1;
}
