#ifndef __HASH_H__
#define __HASH_H__
#include <inttypes.h>

// how to remove the data
typedef void (*destructor_t)(void *);

typedef struct {
  uint64_t key;
  void *val;
} hash_table_data_t;

typedef struct {
  destructor_t data_delete;
  hash_table_data_t **data;
  int size,full;
} hash_table_t;

hash_table_t *hash_table_t_new(int start_size, destructor_t destructor);
void hash_table_t_delete(hash_table_t *r);

void hash_put(hash_table_t *t, uint64_t key, void *data);
void *hash_get(hash_table_t *t, uint64_t key);
void hash_remove(hash_table_t *t,uint64_t key);

uint64_t *get_all_keys();

#endif
