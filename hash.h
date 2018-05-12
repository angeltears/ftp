//
// Created by onter on 18-5-10.
//
#ifndef _HASH_H_
#define _HASH_H_
#include "common.h"

typedef struct hash_node {
    void *key;
    void *value;
    struct hash_node *prev;
    struct hash_node *next;
} hash_node_t;


class hash
{
public:
    typedef hash hash_t;
    typedef unsigned int (*hashfunc_t)(unsigned int, void*);
public:
    hash(unsigned int buckets, hashfunc_t hash_func):buckets(buckets),hash_func(hash_func)
    {
        hash_alloc();
    }
public:
    void hash_alloc();
    void* hash_lookup_entry(void* key, unsigned int key_size);
    void hash_add_entry(void *key, unsigned int key_size,
                        void *value, unsigned int value_size);
    void hash_free_entry(void *key, unsigned int key_size);
private:
    hash_node_t** hash_get_bucket( void *key);
    hash_node_t* hash_get_node_by_key(void *key, unsigned int key_size);
private:
    unsigned int buckets;
    hashfunc_t hash_func;
    hash_node_t **nodes;
};


#endif /* _HASH_H_ */
