#include "hash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static Hash_node_t ** hash_get_bucket(Hash_t *hash, void *key);
static Hash_node_t *hash_get_node_by_key(Hash_t *hash, void *key, unsigned int key_size);
static void hash_clear_bucket(Hash_t *hash, unsigned int index);

Hash_t *hash_alloc(unsigned int buckets, hashfunc_t hash_func)
{
    Hash_t *hash = (Hash_t *)malloc(sizeof(Hash_t));
    if (hash != 0) {
        hash->buckets = buckets; 
        hash->hash_func = hash_func; 
        int len = sizeof(Hash_node_t*) * buckets;
        hash->nodes = (Hash_node_t**)malloc(len);
        if (hash->nodes != 0) {
            memset(hash->nodes, 0, len);
        }
    }
    return hash;
}

void *hash_lookup_value_by_key(Hash_t *hash, 
        void *key, 
        unsigned int key_size)
{
    Hash_node_t *node = hash_get_node_by_key(hash, key, key_size);
    if(node == NULL)
    {
        return NULL;
    }
    else
    {
        return node->value;
    }
}

void hash_add_entry(Hash_t *hash, 
        void *key, 
        unsigned int key_size, 
        void *value,
        unsigned int value_size)
{
    Hash_node_t **buck = hash_get_bucket(hash, key);
    assert(buck != NULL);

    Hash_node_t *node = (Hash_node_t *)malloc(sizeof(Hash_node_t));
    if (node != 0) {
        memset(node, 0, sizeof(Hash_node_t));
        node->key = malloc(key_size);
        node->value = malloc(value_size);
        if(node->key)
            memcpy(node->key, key, key_size);
        if(node->value)
            memcpy(node->value, value, value_size);

        if (*buck != NULL)
        {
            Hash_node_t* head = *buck;

            node->next = head;
            head->pre = node;
            *buck = node; 
        }
        else
        {
            *buck = node;
        }
    }
}

void hash_free_entry(Hash_t *hash, 
        void *key, 
        unsigned int key_size)
{
    Hash_node_t *node = hash_get_node_by_key(hash, key, key_size);
    assert(node != NULL);

    free(node->key);
    free(node->value);

    if(node->pre != NULL)
    {
        node->pre->next = node->next;
    }
    else
    {
        Hash_node_t **buck = hash_get_bucket(hash, key);
        *buck = node->next;
    }
    if(node->next != NULL) 
        node->next->pre = node->pre;
    free(node);
}

void hash_clear_entry(Hash_t *hash)
{
    unsigned int i;
    for(i = 0; i < hash->buckets; ++i)
    {
        hash_clear_bucket(hash, i);
        hash->nodes[i] = NULL;
    }
}

void hash_destroy(Hash_t *hash)
{
    hash_clear_entry(hash);
    free(hash->nodes);
    free(hash);
}

static Hash_node_t **hash_get_bucket(Hash_t *hash, void *key)
{
    unsigned int pos = hash->hash_func(hash->buckets, key); 
    assert(pos < hash->buckets);

    return &(hash->nodes[pos]); 
}

static Hash_node_t *hash_get_node_by_key(Hash_t *hash, 
        void *key, 
        unsigned int key_size)
{
    Hash_node_t **buck = hash_get_bucket(hash, key);
    assert(buck != NULL);

    Hash_node_t *node = *buck;
    while(node != NULL && memcmp(node->key, key, key_size) != 0)
    {
        node = node->next;
    }

    return node; 
}

static void hash_clear_bucket(Hash_t *hash, unsigned int index)
{
    assert(index < hash->buckets); 
    Hash_node_t *node = hash->nodes[index]; 
    Hash_node_t *tmp = node;
    while(node)
    {
        tmp = node->next;
        free(node->key);
        free(node->value);
        free(node);
        node = tmp;
    }
    hash->nodes[index] = NULL;
}

