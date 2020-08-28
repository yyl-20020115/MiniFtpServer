#include "ftp_assist.h"
#include "common.h"
#include "sysutil.h"
#include "session.h"
#include "configure.h"
#include "parse_conf.h"
#include "ftp_codes.h"
#include "command_map.h"
#include "hash.h"

unsigned int num_of_clients = 0;

Hash_t *ip_to_clients = 0;
Hash_t *tid_to_ip = 0;

static unsigned int hash_func(unsigned int buckets, void *key);
static unsigned int add_ip_to_hash(uint32_t ip);

void print_conf()
{
    printf("tunable_pasv_enable=%d\n", tunable_pasv_enable);
    printf("tunable_port_enable=%d\n", tunable_port_enable);

    printf("tunable_listen_port=%u\n", tunable_listen_port);
    printf("tunable_max_clients=%u\n", tunable_max_clients);
    printf("tunable_max_per_ip=%u\n", tunable_max_per_ip);
    printf("tunable_accept_timeout=%u\n", tunable_accept_timeout);
    printf("tunable_connect_timeout=%u\n", tunable_connect_timeout);
    printf("tunable_idle_session_timeout=%u\n", tunable_idle_session_timeout);
    printf("tunable_data_connection_timeout=%u\n", tunable_data_connection_timeout);
    printf("tunable_local_umask=0%o\n", tunable_local_umask);
    printf("tunable_upload_max_rate=%u\n", tunable_upload_max_rate);
    printf("tunable_download_max_rate=%u\n", tunable_download_max_rate);

    if (tunable_listen_address == NULL)
        printf("tunable_listen_address=NULL\n");
    else
        printf("tunable_listen_address=%s\n", tunable_listen_address);
}

#define H1 "There are too many connected users, please try later."
#define H2 "There are too many connections from your internet address."

void limit_num_clients(Session_t *session)
{
    if(tunable_max_clients > 0 && session->curr_clients > tunable_max_clients)
    {
        //421 There are too many connected users, please try later.
        ftp_reply(session, FTP_TOO_MANY_USERS, H1);
        exit_with_error(H1);
        return;
    }

    if(tunable_max_per_ip > 0 && session->curr_ip_clients > tunable_max_per_ip)
    {
        //421 There are too many connections from your internet address.
        ftp_reply(session, FTP_IP_LIMIT, H2);
        exit_with_error(H2);
        return;
    }
}

static unsigned int hash_func(unsigned int buckets, void *key)
{
    unsigned int *number = (unsigned int*)key;

    return (*number) % buckets;
}

static unsigned int add_ip_to_hash(uint32_t ip)
{
    unsigned int *p_value = (unsigned int *)hash_lookup_value_by_key(ip_to_clients, &ip, sizeof(ip));
    if(p_value == NULL)
    {
        unsigned int value = 1;
        hash_add_entry(ip_to_clients, &ip, sizeof(ip), &value, sizeof(value));
        return 1;
    }
    else
    {
        unsigned int value = *p_value;
        ++value;
        *p_value = value;

        return value;
    }
}


void init_hash()
{
    ip_to_clients = hash_alloc(256, hash_func);
    tid_to_ip = hash_alloc(256, hash_func);
}
void free_hash()
{
    hash_destroy(ip_to_clients);
    hash_destroy(tid_to_ip);
}
void add_clients_to_hash(Session_t *session, uint32_t ip)
{
    ++num_of_clients;
    session->curr_clients = num_of_clients;
    session->curr_ip_clients = add_ip_to_hash(ip);
}

void add_tid_ip_to_hash(tid_t tid, uint32_t ip)
{
    hash_add_entry(tid_to_ip, &tid, sizeof(tid), &ip, sizeof(ip));
}

