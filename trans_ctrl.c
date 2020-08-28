#include "trans_ctrl.h"
#include "common.h"
#include "sysutil.h"
#include "configure.h"
#include "command_map.h"
#include "ftp_codes.h"
#include "strutil.h"
extern Session_t *p_sess = NULL;

static void handle_signal_alarm_ctrl_fd(int sig);
static void handle_signal_alarm_data_fd(int sig);
static void handle_signal_sigurg(int sig);

void setup_signal_alarm_ctrl_fd()
{
    if (signal(SIGALRM, handle_signal_alarm_ctrl_fd) == SIG_ERR) {
        exit_with_error("signal");
    }
}

void start_signal_alarm_ctrl_fd()
{
    alarm(tunable_idle_session_timeout);
}

static void handle_signal_alarm_ctrl_fd(int sig)
{
    if(tunable_idle_session_timeout > 0)
    {
        shutdown(p_sess->peer_fd, SHUT_RD);
        //421
        ftp_reply(p_sess, FTP_IDLE_TIMEOUT, "Timeout.");
        shutdown(p_sess->peer_fd, SHUT_WR);
        exit(EXIT_SUCCESS);
    }
}

void setup_signal_alarm_data_fd()
{
    if (signal(SIGALRM, handle_signal_alarm_data_fd) == SIG_ERR) {
        exit_with_error("signal");
    }
}

void start_signal_alarm_data_fd()
{
    alarm(tunable_data_connection_timeout);
}

static void handle_signal_alarm_data_fd(int sig)
{
    if(tunable_data_connection_timeout > 0)
    {
        if(p_sess->is_translating_data == 1)
        {
            start_signal_alarm_data_fd();
        }
        else
        {
            close(p_sess->data_fd);
            shutdown(p_sess->peer_fd, SHUT_RD);
            ftp_reply(p_sess, FTP_DATA_TIMEOUT, "Timeout.");
            shutdown(p_sess->peer_fd, SHUT_WR);
            exit(EXIT_SUCCESS);
        }
    }
}

void cancel_signal_alarm()
{
    alarm(0);
}

static void handle_signal_sigurg(int sig)
{
    char cmdline[1024] = {0};
    int ret = readline(p_sess->peer_fd, cmdline, sizeof cmdline);
    if (ret <= 0) {
        exit_with_error("readline");
        return;
    }
    str_trim_crlf(cmdline);
    str_upper(cmdline);

    if(strcmp("ABOR", cmdline) == 0 || strcmp("\377\364\377\362ABOR", cmdline) == 0)
    {
        p_sess->is_receive_abor = 1;
        close(p_sess->data_fd);
        p_sess->data_fd = -1;
    }
    else
    {
        ftp_reply(p_sess, FTP_BADCMD, "Unknown command.");
    }
}

void setup_signal_sigurg()
{
    if (signal(SIGURG, handle_signal_sigurg) == SIG_ERR) {
        exit_with_error("signal");
    }
}
