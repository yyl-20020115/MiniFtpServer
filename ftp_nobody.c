#include "ftp_nobody.h"
#include "common.h"
#include "sysutil.h"
#include "priv_command.h"
#include "priv_sock.h"

int handle_nobody(Session_t *session)
{
    int r = 0;
    char cmd = 0;
    while(!should_exit())
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
    return EXIT_SUCCESS;
}
