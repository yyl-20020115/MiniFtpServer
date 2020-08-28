#include "ftp_nobody.h"
#include "common.h"
#include "sysutil.h"
#include "priv_command.h"
#include "priv_sock.h"

#ifndef _WIN32
void set_nobody()
{
    struct passwd* pw;
    if ((pw = getpwnam("nobody")) == NULL) {
        exit_with_error("getpwnam");
        return;
    }
    if (setegid(pw->pw_gid) == -1){
        exit_with_error("setegid");
        return;
    }

    if (seteuid(pw->pw_uid) == -1){
        exit_with_error("seteuid");
        return;
    }
}
void set_bind_capabilities()
{
    struct __user_cap_header_struct cap_user_header;
    cap_user_header.version = _LINUX_CAPABILITY_VERSION_1;
    cap_user_header.pid = getpid();

    struct __user_cap_data_struct cap_user_data;
    __u32 cap_mask = 0; 
    cap_mask |= (1 << CAP_NET_BIND_SERVICE); //0001000000
    cap_user_data.effective = cap_mask;
    cap_user_data.permitted = cap_mask;
    cap_user_data.inheritable = 0;

    if (capset(&cap_user_header, &cap_user_data) == -1) {
        exit_with_error("capset");
    }
}

int capset(cap_user_header_t hdrp, const cap_user_data_t datap)
{
    return syscall(SYS_capset, hdrp, datap);
}
#endif
int handle_nobody(Session_t *session)
{
#ifndef _WIN32
    set_nobody();
    set_bind_capabilities();
#endif
    int r = 0;
    char cmd = 0;
    for(;;)
    {
        r = priv_sock_recv_cmd(session->nobody_fd,&cmd);
        switch (cmd)
        {
        case PRIV_SOCK_EOF:
            return EXIT_SUCCESS;
        case PRIV_SOCK_GET_DATA_SOCK:
            r = privop_pasv_get_data_sock(session);
            break;
        case PRIV_SOCK_PASV_ACTIVE:
            r = privop_pasv_active(session);
            break;
        case PRIV_SOCK_PASV_LISTEN:
            r = privop_pasv_listen(session);
            break;
        case PRIV_SOCK_PASV_ACCEPT:
            r = privop_pasv_accept(session);
            break;
        case PRIV_SOCK_INVALID_COMMAND:
        default:
            exit_with_error("Unkown command\n");
            return EXIT_FAILURE;
        }

        if (r != PRIV_SOCK_OPERATION_SUCCEEDED) {
            //any failed operation would result in exit of the thread loop
            return EXIT_FAILURE;
        }
    }
}
