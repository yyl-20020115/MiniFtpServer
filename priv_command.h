#ifndef _PRIV_COMMAND_H_
#define _PRIV_COMMAND_H_

#include "session.h"

int privop_pasv_get_data_sock(Session_t *session);
int privop_pasv_active(Session_t * session);
int privop_pasv_listen(Session_t * session);
int privop_pasv_accept(Session_t * session);

#endif  /*_PRIV_COMMAND_H_*/
