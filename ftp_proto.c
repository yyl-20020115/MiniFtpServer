#include "ftp_proto.h"
#include "common.h"
#include "sysutil.h"
#include "strutil.h"
#include "ftp_codes.h"
#include "command_map.h"
#include "trans_ctrl.h"

int handle_proto(Session_t *session)
{
    int e = 0;
    ftp_reply(session, FTP_GREET, GREETINGS);
    while(!should_exit())
    {
        session_reset_command(session);

        int ret = readline(session->peer_fd, session->command, MAX_COMMAND);

        if (ret == -1)
        {
            if (
#ifndef _WIN32
                errno == EINTR
#else
                (e = WSAGetLastError()) == WSAEINTR
#endif
                )
                continue;

            else
            {
                exit_with_error("readline");
                return EXIT_FAILURE;
            }
        }
        else if (ret == 0)
        {
            //normally exits
            break;
        }
        str_trim_crlf(session->command);
        str_split(session->command, session->com, session->args, ' ');
        str_upper(session->com);
        printf("COMMD=[%s], ARGS=[%s]\n", session->com, session->args);
        int r = do_command_map(session);
        if (EXIT_FAILURE == r) {
            return EXIT_FAILURE;
        }
        else if (r == QUIT_SUCCESS) //here QUIT_SUCCESS means quit
        {
            break;
        }
    }
    return EXIT_SUCCESS;
}
