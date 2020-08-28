#ifndef _HASH_H_
#define _HASH_H_

typedef unsigned int (*hashfunc_t)(unsigned int, void*);

typedef struct Hash_node
{
    void *key; 
    void *value; 
    struct Hash_node *pre;
    struct Hash_node *next;
}Hash_node_t;   

typedef struct Hash
{
    unsigned int buckets; 
    hashfunc_t hash_func; 
    Hash_node_t **nodes; 
}Hash_t;


Hash_t *hash_alloc(unsigned int buckets, hashfunc_t hash_func);
void *hash_lookup_value_by_key(Hash_t *hash, void *key, unsigned int key_size);
void hash_add_entry(Hash_t *hash, void *key, unsigned int key_size, void *value, unsigned int value_size);
void hash_free_entry(Hash_t *hash, void *key, unsigned int key_size);
void hash_clear_entry(Hash_t *hash);
void hash_destroy(Hash_t *hash);

#endif  /*_HASH_H_*/
