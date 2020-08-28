#ifndef _TRANS_CTRL_H_
#define _TRANS_CTRL_H_

#include "session.h"

void limit_curr_rate(Session_t *session, int nbytes, int is_upload);

void setup_signal_alarm_ctrl_fd();
void start_signal_alarm_ctrl_fd();

void setup_signal_alarm_data_fd();
void start_signal_alarm_data_fd();

void cancel_signal_alarm();

void setup_signal_sigurg();

int do_site_chmod(Session_t *session, char *args);
int do_site_umask(Session_t *session, char *args);
int do_site_help(Session_t *session);

#endif 