#ifndef _COMMAND_MAP_H_
#define _COMMAND_MAP_H_

#include "session.h"

int do_command_map(Session_t * session);
void ftp_reply(Session_t * session, int, const char *);
void ftp_lreply(Session_t * session, int, const char *);

int do_user(Session_t * session);
int do_pass(Session_t * session);
int do_cwd(Session_t * session);
int do_cdup(Session_t * session);
int do_quit(Session_t * session);
int do_port(Session_t * session);
int do_pasv(Session_t * session);
int do_type(Session_t * session);
int do_stru(Session_t * session);
int do_mode(Session_t * session);
int do_retr(Session_t * session);
int do_stor(Session_t * session);
int do_appe(Session_t * session);
int do_list(Session_t * session);
int do_nlst(Session_t * session);
int do_rest(Session_t * session);
int do_abor(Session_t * session);
int do_pwd(Session_t * session);
int do_mkd(Session_t * session);
int do_rmd(Session_t * session);
int do_dele(Session_t * session);
int do_rnfr(Session_t * session);
int do_rnto(Session_t * session);
int do_site(Session_t * session);
int do_syst(Session_t * session);
int do_feat(Session_t * session);
int do_size(Session_t * session);
int do_stat(Session_t * session);
int do_noop(Session_t * session);
int do_help(Session_t * session);

#endif 
