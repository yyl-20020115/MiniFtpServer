#ifndef _TRANS_DATA_H_
#define _TRANS_DATA_H_

#include "session.h"

int download_file(Session_t * session);
int upload_file(Session_t * session, int appending);
int trans_list(Session_t * session, int list);

#endif  /*_TRANS_DATA_H_*/
