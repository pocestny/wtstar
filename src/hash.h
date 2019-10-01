/**
 * @file hash.h
 * @brief simple hashmap to store values with `uint64_t` keys
 */
#ifndef __HASH_H__
#define __HASH_H__
#include <inttypes.h>

//! handler to dispose of the data when the table is detoryed
typedef void (*destructor_t)(void *);

//! key-value pair
typedef struct {
  uint64_t key;
  void *val;
} hash_table_data_t;

//! hash table structure
typedef struct {
  destructor_t data_delete; //<! function to delete the values
  hash_table_data_t **data; //<! allocated array of key-value pairs
  int size,                 //<! allocated size
      full;                 //<! number of elements
} hash_table_t;

//! constructs a new hash table
hash_table_t *hash_table_t_new(int start_size, destructor_t destructor);

//! frees the table and if destructor is not NULL, calls it on every value
void hash_table_t_delete(hash_table_t *r);

//! store a value in the table
void hash_put(hash_table_t *t, uint64_t key, void *data);

//! retireve a value (or NULL) with given `key`
void *hash_get(hash_table_t *t, uint64_t key);

//! remove the value with `key` (if present)
void hash_remove(hash_table_t *t,uint64_t key);

#endif
