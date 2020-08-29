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

