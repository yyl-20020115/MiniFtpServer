#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <assert.h>

#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#include <WS2tcpip.h>
typedef int uid_t;
typedef int tid_t;
typedef int mode_t;
#ifdef _WIN64
typedef int ssize_t;
#else
typedef long long ssize_t;
#endif
ssize_t send_file_block(SOCKET out_fd, int in_fd, long long* offset, size_t count);
int nanosleep(const struct timespec *req, struct timespec *rem);
int inet_aton(const char* cp, struct in_addr* inp);
#include <Windows.h>
#include <io.h>
#else
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/capability.h>
#include <sys/syscall.h>
#include <bits/syscall.h>
#include <sys/sendfile.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <pwd.h>
#include <shadow.h>
#include "crypt.h"
#include <dirent.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
typedef int SOCKET;
#endif

int should_exit();
int exit_with_code(int code);
int exit_with_error(const char* format, ...);
int exit_with_message(const char* format, ...);
int s_close(SOCKET* s);
int s_timeout();

#define QUIT_SUCCESS (-1)
#define GREETINGS "(FtpServer 1.0)"
#define UNKNOWN_CMD "Unknown command."
#define UNIMPL_CMD "Unimplement command."

#endif