#include "configure.h"

int tunable_pasv_enable = 1;
int tunable_port_enable = 1;
unsigned int tunable_listen_port = 8989;
unsigned int tunable_max_clients = 2000;
unsigned int tunable_max_per_ip = 50;
unsigned int tunable_accept_timeout = 60;
unsigned int tunable_connect_timeout = 60;
unsigned int tunable_idle_session_timeout = 500;
unsigned int tunable_data_connection_timeout = 900;
unsigned int tunable_local_umask = 077;
unsigned int tunable_upload_max_rate = 0;
unsigned int tunable_download_max_rate = 0;
const char *tunable_listen_address = "127.0.0.1";

