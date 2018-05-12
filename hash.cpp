//
// Created by onter on 18-5-10.
//

#include "common.h"
#include "hash.h"


void hash::hash_alloc()
{
    int size = buckets * sizeof(hash_node_t *);
    nodes = new hash_node_t* [buckets];
    memset(nodes, 0, size);
}

void* hash::hash_lookup_entry(void* key, unsigned int key_size)
{
    hash_node_t * _node = hash_get_node_by_key(key, key_size);
    if (_node == NULL)
    {
        return NULL;
    }
    return _node->value;
}


void hash::hash_add_entry(void *key, unsigned int key_size,
                          void *value, unsigned int value_size)
{
    if (hash_lookup_entry(key, key_size) != NULL)
    {
        fprintf(stderr, "duplicate hash key\n");
        return;
    }

    hash_node_t *_node = new hash_node_t;
    _node->next = NULL;
    _node->prev = NULL;
    _node->value = malloc(value_size);
    memcpy(_node->value, value, value_size);
    _node->key = malloc(key_size);
    memcpy(_node->key, key, key_size);

    hash_node_t **_head = hash_get_bucket(key);
    if (*_head == NULL)
    {
        *_head = _node;
        return;
    }
    else
    {
        _node->next = *_head;
        (*_head)->prev = _node;
        *_head = _node;
    }
}

void hash::hash_free_entry(void *key, unsigned int key_size)
{
    hash_node_t *_node = hash_get_node_by_key(key, key_size);
    if (_node == NULL)
    {
        return;
    }

    free(_node->key);
    free(_node->value);

    if (_node->prev != NULL)
    {
        _node->prev->next = _node->next;
    }
    else
    {
        hash_node_t **_head = hash_get_bucket(key);
        *_head = _node->next;
    }

    if(_node ->next != NULL)
    {
        _node->next->prev = _node->prev;
    }

    delete _node;
}

hash_node_t** hash::hash_get_bucket(void *key)
{
    unsigned int _buckets = hash_func(buckets, key);
    if (_buckets > buckets)
    {
        fprintf(stderr,"hash_get_bucket erron");
        exit(EXIT_FAILURE);
    }
    return &nodes[_buckets];
}
hash_node_t* hash::hash_get_node_by_key(void *key, unsigned int key_size)
{
    hash_node_t ** _head = hash_get_bucket(key);
    hash_node_t * _node = *_head;
    if (_node == NULL)
    {
        return NULL;
    }

    while(_node != NULL && memcmp(_node->key, key, key_size))
    {
        _node = _node->next;
    }
    return _node;
}