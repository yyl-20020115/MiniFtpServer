#ifndef __SESSION_MANAGER_H__
#define __SESSION_MANAGER_H__
#include "session.h"

int init_session_manager();
int destroy_session_manager();

int add_session(Session_t* session);
int remove_session(Session_t* session);

int wait_sessions();

#endif