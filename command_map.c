#include "command_map.h"
#include "common.h"
#include "session.h"
#include "sysutil.h"
#include "ftp_codes.h"
#include "configure.h"
#include "trans_data.h"
#include "priv_sock.h"
#include "strutil.h"
#include "trans_ctrl.h"

typedef struct _ftpcmd_t
{
    const char* cmd;
    int (*cmd_handler)(Session_t* session);
} ftpcmd_t;

static ftpcmd_t ftp_cmds[] = {
    {"USER", do_user },
    {"PASS", do_pass },
    {"CWD", do_cwd },
    {"XCWD", do_cwd },
    {"CDUP", do_cdup },
    {"XCUP", do_cdup },
    {"QUIT", do_quit },
    {"ACCT", NULL },
    {"SMNT", NULL },
    {"REIN", NULL },
    {"PORT", do_port },
    {"PASV", do_pasv },
    {"TYPE", do_type },
    {"STRU", do_stru },
    {"MODE", do_mode },

    {"RETR", do_retr },
    {"STOR", do_stor },
    {"APPE", do_appe },
    {"LIST", do_list },
    {"NLST", do_nlst },
    {"REST", do_rest },
    {"ABOR", do_abor },
    {"\377\364\377\362ABOR", do_abor},
    {"PWD", do_pwd },
    {"XPWD", do_pwd },
    {"MKD", do_mkd },
    {"XMKD", do_mkd },
    {"RMD", do_rmd },
    {"XRMD", do_rmd },
    {"DELE", do_dele },
    {"RNFR", do_rnfr },
    {"RNTO", do_rnto },
    {"SITE", do_site },
    {"SYST", do_syst },
    {"FEAT", do_feat },
    {"SIZE", do_size },
    {"STAT", do_stat },
    {"NOOP", do_noop },
    {"HELP", do_help },
    {"STOU", NULL },
    {"ALLO", NULL }
};

int do_command_map(Session_t* session)
{
    int i = 0, r = 0;
    int size = sizeof(ftp_cmds) / sizeof(ftp_cmds[0]);
    for (i = 0; i < size; ++i)
    {
        if (strcmp(ftp_cmds[i].cmd, session->com) == 0)
        {
            if (ftp_cmds[i].cmd_handler != NULL)
            {
                r = ftp_cmds[i].cmd_handler(session);
            }
            else
            {
                ftp_reply(session, FTP_COMMANDNOTIMPL, UNIMPL_CMD);
            }
            break;
        }
    }

    if (i == size)
    {
        ftp_reply(session, FTP_BADCMD, UNKNOWN_CMD);
        //return EXIT_FAILURE;
    }
    else if (r != 0) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

void ftp_reply(Session_t* session, int status, const char* text)
{
    char _text[1024] = { 0 };
    snprintf(_text, sizeof(_text), "%d %s\r\n", status, text);
    writes(session->peer_fd, _text);
}

void ftp_lreply(Session_t* session, int status, const char* text)
{
    char _text[1024] = { 0 };
    snprintf(_text, sizeof(_text), "%d-%s\r\n", status, text);
    writes(session->peer_fd, _text);
}

int do_user(Session_t* session)
{
#ifndef _WIN32
    struct passwd* pw;
    if ((pw = getpwnam(session->args)) == NULL)
    {
        ftp_reply(session, FTP_LOGINERR, "Login incorrect.");
        return EXIT_SUCCESS;
    }

    session->user_uid = pw->pw_uid;
#endif
    ftp_reply(session, FTP_GIVEPWORD, "Please specify the password.");
    return EXIT_SUCCESS;
}

int do_pass(Session_t* session)
{
#ifndef _WIN32
    struct passwd* pw;
    if ((pw = getpwuid(session->user_uid)) == NULL)
    {
        ftp_reply(session, FTP_LOGINERR, "Login incorrect.");
        return EXIT_SUCCESS;
    }

    //struct spwd *getspnam(const char *name);
    struct spwd* spw;
    if ((spw = getspnam(pw->pw_name)) == NULL)
    {
        ftp_reply(session, FTP_LOGINERR, "Login incorrect.");
        return EXIT_SUCCESS;
    }

    //char *crypt(const char *key, const char *salt);
    char* encrypted_password = crypt(session->args, spw->sp_pwdp);
    if (strcmp(encrypted_password, spw->sp_pwdp) != 0)
    {
        ftp_reply(session, FTP_LOGINERR, "Login incorrect.");
        return EXIT_SUCCESS;
    }

    if (setegid(pw->pw_gid) == -1) 
    {
        exit_with_error("setegid");
        return EXIT_SUCCESS;
    }
    if (seteuid(pw->pw_uid) == -1) 
    {
        exit_with_error("seteuid");
        return EXIT_SUCCESS;
    }

    if (chdir(pw->pw_dir) == -1) 
    {
        exit_with_error("chdir");
        return EXIT_SUCCESS;
    }

    umask(tunable_local_umask);

    strcpy(session->username, pw->pw_name);
#endif
    ftp_reply(session, FTP_LOGINOK, "Login successful.");
    return EXIT_SUCCESS;
}


int do_cwd(Session_t* session)
{
    if (_chdir(session->args) == -1)
    {
        //550
        ftp_reply(session, FTP_FILEFAIL, "Failed to change directory.");
        return EXIT_SUCCESS;
    }

    //250 Directory successfully changed.
    ftp_reply(session, FTP_CWDOK, "Directory successfully changed.");
    return EXIT_SUCCESS;
}

int do_cdup(Session_t* session)
{
    if (_chdir("..") == -1)
    {
        //550
        ftp_reply(session, FTP_FILEFAIL, "Failed to change directory.");
        return EXIT_SUCCESS;
    }

    //250 Directory successfully changed.
    ftp_reply(session, FTP_CWDOK, "Directory successfully changed.");
    return EXIT_SUCCESS;
}

int do_quit(Session_t* session)
{
    ftp_reply(session, FTP_GOODBYE, "Good Bye!");

    return QUIT_SUCCESS;
}

int do_port(Session_t* session)
{
    //PORT 192,168,44,1,200,174
    unsigned int v[6] = { 0 };
    int r = sscanf(session->args, "%u,%u,%u,%u,%u,%u", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]);
    if (r == 6) {
        session->p_addr = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
        if (session->p_addr != 0) {
            memset(session->p_addr, 0, sizeof(struct sockaddr_in));
            session->p_addr->sin_family = AF_INET;
            char* p = (char*)&session->p_addr->sin_port;
            p[0] = v[4];
            p[1] = v[5];

            p = (char*)&session->p_addr->sin_addr.s_addr;
            p[0] = v[0];
            p[1] = v[1];
            p[2] = v[2];
            p[3] = v[3];
            ftp_reply(session, FTP_PORTOK, "PORT command successful. Consider using PASV.");
        }
    }
    else
    {
        ftp_reply(session, FTP_BADCMD, "Bad command.");
    }
    return EXIT_SUCCESS;
}

int do_pasv(Session_t* session)
{
    char ip[16] = { 0 };
    get_local_ip(ip,sizeof(ip));

    priv_sock_send_cmd(session->proto_fd, PRIV_SOCK_PASV_LISTEN);
    
    char res = 0;
    priv_sock_recv_result(session->proto_fd,&res);
    
    if (res == PRIV_SOCK_RESULT_BAD)
    {
        ftp_reply(session, FTP_BADCMD, "get listenfd error");
        return EXIT_SUCCESS;
    }
    uint16_t port = 0;
    int res2 = 0;
    priv_sock_recv_int(session->proto_fd,&res2);
    port = res2;
    //227 Entering Passive Mode (192,168,44,136,194,6).
    unsigned int v[6] = { 0 };

    int r = sscanf(ip, "%u.%u.%u.%u", &v[0], &v[1], &v[2], &v[3]);
    if (r == 4) {
        uint16_t net_endian_port = htons(port);
        unsigned char* p = (unsigned char*)&net_endian_port;
        v[4] = p[0];
        v[5] = p[1];

        char text[1024] = { 0 };
        snprintf(text, sizeof text, "Entering Passive Mode (%u,%u,%u,%u,%u,%u).", v[0], v[1], v[2], v[3], v[4], v[5]);

        ftp_reply(session, FTP_PASVOK, text);
    }
    else 
    {
        ftp_reply(session, FTP_BADCMD, "Bad command.");
    }
    return EXIT_SUCCESS;

}

int do_type(Session_t* session)
{
    if (strcmp(session->args, "A") == 0)
    {
        session->ascii_mode = 1;
        ftp_reply(session, FTP_TYPEOK, "Switching to ASCII mode.");
    }
    else if (strcmp(session->args, "I") == 0)
    {
        session->ascii_mode = 0;
        ftp_reply(session, FTP_TYPEOK, "Switching to Binary mode.");
    }
    else
    {
        ftp_reply(session, FTP_BADCMD, "Unrecognised TYPE command.");
    }
    return EXIT_SUCCESS;
}

int do_stru(Session_t* session)
{
    ftp_reply(session, FTP_COMMANDNOTIMPL, UNIMPL_CMD);
    return EXIT_SUCCESS;
}

int do_mode(Session_t* session)
{
    ftp_reply(session, FTP_COMMANDNOTIMPL, UNIMPL_CMD);
    return EXIT_SUCCESS;
}

int do_retr(Session_t* session)
{
    return download_file(session);
}

int do_stor(Session_t* session)
{
    return upload_file(session, 0);
}

int do_appe(Session_t* session)
{
    return upload_file(session, 1);
}

int do_list(Session_t* session)
{
    trans_list(session, 1);
    return EXIT_SUCCESS;
}

int do_nlst(Session_t* session)
{
    trans_list(session, 0);
    return EXIT_SUCCESS;
}

int do_rest(Session_t* session)
{
    session->restart_pos = atoll(session->args);
    //Restart position accepted (344545).
    char text[1024] = { 0 };
    snprintf(text, sizeof(text), "Restart position accepted (%lld).", session->restart_pos);
    ftp_reply(session, FTP_RESTOK, text);
    return EXIT_SUCCESS;
}

int do_abor(Session_t* session)
{
    //225 No transfer to ABOR
    ftp_reply(session, FTP_ABOR_NOCONN, "No transfer to ABOR.");
    return EXIT_SUCCESS;
}

int do_pwd(Session_t* session)
{
    char tmp[1024] = { 0 };
    if (_getcwd(tmp, sizeof tmp) == NULL)
    {
        ftp_reply(session, FTP_BADMODE, "get cwd error");
        return EXIT_SUCCESS;
    }
    char text[1024] = { 0 };
    snprintf(text, sizeof (text), "\"%s\"", tmp);
    ftp_reply(session, FTP_PWDOK, text);
    return EXIT_SUCCESS;
}

int do_mkd(Session_t* session)
{
#ifndef _WIN32
    if (mkdir(session->args, 0777) == -1)
#else
    if (_mkdir(session->args) == -1)
#endif
    {
        ftp_reply(session, FTP_FILEFAIL, "Create directory operation failed.");
        return EXIT_SUCCESS;
    }

    char text[1024] = { 0 };
    if (session->args[0] == '/')
    {
        snprintf(text, sizeof(text), "%s created.", session->args);
    }
    else
    {
        char tmp[1024] = { 0 };
        if (_getcwd(tmp, sizeof(tmp)) == NULL)
        {
            exit_with_error("getcwd");
            return EXIT_SUCCESS;
        }
        snprintf(text, sizeof text, "%s/%s created.", tmp, session->args);
    }

    ftp_reply(session, FTP_MKDIROK, text);
    return EXIT_SUCCESS;
}

int do_rmd(Session_t* session)
{
    if (_rmdir(session->args) == -1)
    {
        //550 Remove directory operation failed.
        ftp_reply(session, FTP_FILEFAIL, "Remove directory operation failed.");
        return EXIT_SUCCESS;
    }
    //250 Remove directory operation successful.
    ftp_reply(session, FTP_RMDIROK, "Remove directory operation successful.");
    return EXIT_SUCCESS;
}

int do_dele(Session_t* session)
{
    if (_unlink(session->args) == -1)
    {
        //550 Delete operation failed.
        ftp_reply(session, FTP_FILEFAIL, "Delete operation failed.");
        return EXIT_SUCCESS;
    }
    //250 Delete operation successful.
    ftp_reply(session, FTP_DELEOK, "Delete operation successful.");
    return EXIT_SUCCESS;
}

int do_rnfr(Session_t* session)
{
    if (session->rnfr_name)
    {
        free(session->rnfr_name);
        session->rnfr_name = NULL;
    }
    size_t len = strlen(session->args);
    session->rnfr_name = (char*)malloc(len + 1);
    if (session->rnfr_name) {
        memset(session->rnfr_name, 0, len + 1);
        strcpy(session->rnfr_name, session->args);

        //350 Ready for RNTO.
    }
    ftp_reply(session, FTP_RNFROK, "Ready for RNTO.");
    return EXIT_SUCCESS;
}

int do_rnto(Session_t* session)
{
    if (session->rnfr_name == NULL)
    {
        //503 RNFR required first.
        ftp_reply(session, FTP_NEEDRNFR, "RNFR required first.");
        return EXIT_SUCCESS;
    }

    if (rename(session->rnfr_name, session->args) == -1)
    {
        ftp_reply(session, FTP_FILEFAIL, "Rename failed.");
        return EXIT_SUCCESS;
    }
    if (session->rnfr_name != NULL) {
        free(session->rnfr_name);
        session->rnfr_name = NULL;
    }
    //250 Rename successful.
    ftp_reply(session, FTP_RENAMEOK, "Rename successful.");
    return EXIT_SUCCESS;
}

int do_site_chmod(Session_t* session, char* args)
{
    if (strlen(args) == 0)
    {
        ftp_reply(session, FTP_BADCMD, "SITE CHMOD needs 2 arguments.");
        return EXIT_SUCCESS;
    }

    char perm[256] = { 0 };
    char file[256] = { 0 };
    str_split(args, perm, file, ' ');
    if (strlen(file) == 0)
    {
        ftp_reply(session, FTP_BADCMD, "SITE CHMOD needs 2 arguments.");
        return EXIT_SUCCESS;
    }

    unsigned int mode = str_octal_to_uint(perm);
#ifndef _WIN32
    if (chmod(file, mode) < 0)
    {
        ftp_reply(session, FTP_CHMODOK, "SITE CHMOD command failed.");
    }
    else
    {
        ftp_reply(session, FTP_CHMODOK, "SITE CHMOD command ok.");
    }
#else
    ftp_reply(session, FTP_CHMODOK, "SITE CHMOD command ok.");
#endif
    return EXIT_SUCCESS;
}

int do_site_umask(Session_t* session, char* args)
{
    // SITE UMASK [umask]
    if (strlen(args) == 0)
    {
        char text[1024] = { 0 };
        sprintf(text, "Your current UMASK is 0%o", tunable_local_umask);
        ftp_reply(session, FTP_UMASKOK, text);
    }
    else
    {
        unsigned int um = str_octal_to_uint(args);
#ifndef _WIN32
        umask(um);
#endif
        char text[1024] = { 0 };
        sprintf(text, "UMASK set to 0%o", um);
        ftp_reply(session, FTP_UMASKOK, text);
    }
    return EXIT_SUCCESS;
}

int do_site_help(Session_t* session)
{
    //214 CHMOD UMASK HELP
    ftp_reply(session, FTP_HELP, "CHMOD UMASK HELP");
    return EXIT_SUCCESS;
}


int do_site(Session_t* session)
{
    char cmd[1024] = { 0 };
    char args[1024] = { 0 };
    str_split(session->args, cmd, args, ' ');
    str_upper(cmd);

    if (strcmp("CHMOD", cmd))
        return do_site_chmod(session, args);
    else if (strcmp("UMASK", cmd))
        return do_site_umask(session, args);
    else if (strcmp("HELP", cmd))
        return do_site_help(session);
    else
        ftp_reply(session, FTP_BADCMD, "Unknown SITE command.");
    return EXIT_SUCCESS;
}

int do_syst(Session_t* session)
{
#ifndef _WIN32
    ftp_reply(session, FTP_SYSTOK, "UNIX Type: L8");
#else
    ftp_reply(session, FTP_SYSTOK, "Windows");
#endif
    return EXIT_SUCCESS;
}

int do_feat(Session_t* session)
{
    //211-Features:
    ftp_lreply(session, FTP_FEAT, "Features:");

    //EPRT
    writes(session->peer_fd, " EPRT\r\n");
    writes(session->peer_fd, " EPSV\r\n");
    writes(session->peer_fd, " MDTM\r\n");
    writes(session->peer_fd, " PASV\r\n");
    writes(session->peer_fd, " REST STREAM\r\n");
    writes(session->peer_fd, " SIZE\r\n");
    writes(session->peer_fd, " TVFS\r\n");
    writes(session->peer_fd, " UTF8\r\n");

    //211 End
    ftp_reply(session, FTP_FEAT, "End");
    return EXIT_SUCCESS;
}

int do_size(Session_t* session)
{
    struct stat sbuf = { 0 };
#ifndef _WIN32
    if (lstat(session->args, &sbuf) == -1)
#else
    if (stat(session->args, &sbuf) == -1)
#endif
    {
        ftp_reply(session, FTP_FILEFAIL, "SIZE operation failed.");
        return EXIT_SUCCESS;
    }

    if ((sbuf.st_mode & _S_IFREG) == 0)
    {
        ftp_reply(session, FTP_FILEFAIL, "SIZE operation failed.");
        return EXIT_SUCCESS;
    }
    //213 6
    char text[1024] = { 0 };
    snprintf(text, sizeof text, "%lu", sbuf.st_size);
    ftp_reply(session, FTP_SIZEOK, text);
    return EXIT_SUCCESS;
}

int do_stat(Session_t* session)
{
    ftp_lreply(session, FTP_STATOK, "FTP server status:");

    char text[1024] = { 0 };
    struct in_addr in = { 0 };
    in.s_addr = session->ip;
    snprintf(text, sizeof text, " Connected to %s\r\n", inet_ntoa(in));
    writes(session->peer_fd, text);

    snprintf(text, sizeof text, " Logged in as %s\r\n", session->username);
    writes(session->peer_fd, text);
    ftp_reply(session, FTP_STATOK, "End of status");
    return EXIT_SUCCESS;
}

int do_noop(Session_t* session)
{
    ftp_reply(session, FTP_GREET, GREETINGS);
    return EXIT_SUCCESS;
}
#define H0 "The following commands are recognized."
#define H1 " ABOR ACCT ALLO APPE CDUP CWD DELE EPRT EPSV FEAT HELP LIST MDTM MKD\r\n"
#define H2 " MODE NLST NOOP OPTS PASS PASV PORT PWD QUIT REIN REST RETR RMD RNFR\r\n"
#define H3 " RNTO SITE SIZE SMNT STAT STOR STOU STRU SYST TYPE USER XCUP XCWD XMKD\r\n"
#define H4 " XPWD XRMD\r\n"
#define H5 "Help OK."
int do_help(Session_t* session)
{
    ftp_lreply(session, FTP_HELP, H0);
    writes(session->peer_fd, H1);
    writes(session->peer_fd, H2);
    writes(session->peer_fd, H3);
    writes(session->peer_fd, H4);
    ftp_reply(session, FTP_HELP, H5);
    return EXIT_SUCCESS;
}
