#ifndef _SYS_UTIL_H_
#define _SYS_UTIL_H_

#include "common.h"

void activate_oobinline(int sockfd);
void activate_signal_sigurg(int sockfd);

int get_curr_time_sec();
int get_curr_time_usec();
int n_sleep(double t);

int lock_file_read(int fd);
int lock_file_write(int fd);
int unlock_file(int fd);

SOCKET tcp_client(unsigned int port);
SOCKET tcp_server(const char *host, unsigned short port);

int get_local_ip(char *ip);

void activate_nonblock(SOCKET fd);
void deactivate_nonblock(SOCKET fd);

int read_timeout(SOCKET fd, unsigned int wait_seconds);
int write_timeout(SOCKET fd, unsigned int wait_seconds);
SOCKET accept_timeout(SOCKET fd, struct sockaddr_in *addr, unsigned int wait_seconds);
int connect_timeout(SOCKET fd, struct sockaddr_in *addr, unsigned int wait_seconds);

ssize_t readn(SOCKET fd, void *buf, size_t count);
ssize_t writes(SOCKET fd, const char* text);
ssize_t writen(SOCKET fd, const void *buf, size_t n);
ssize_t readline(SOCKET sockfd, void *buf, size_t maxline);

#endif

