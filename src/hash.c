/**
 * @file hash.c
 * @brief Implementation of hash.h
 */
#include <hash.h>
#include <stdlib.h>

static uint64_t __hash(uint64_t x) {
  x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
  x = x ^ (x >> 31);
  return x;
}

hash_table_t *hash_table_t_new(int start_size, destructor_t destructor) {
  hash_table_t *r = (hash_table_t *)malloc(sizeof(hash_table_t));
  r->data_delete = destructor;

  if (start_size < 16) start_size = 16;
  r->size = start_size;

  r->data = (hash_table_data_t **)malloc(r->size * sizeof(hash_table_data_t *));
  for (int i = 0; i < r->size; i++) r->data[i] = NULL;

  r->full = 0;
  return r;
}

void hash_table_t_delete(hash_table_t *r) {
  for (int i = 0; i < r->size; i++)
    if (r->data[i]) {
      if (r->data_delete) r->data_delete(r->data[i]->val);
      free(r->data[i]);
    }
  free(r->data);
  free(r);
}

static void __insert(uint64_t key, void *val, int size,
                     hash_table_data_t **data, destructor_t data_delete) {
  uint64_t h = __hash(key) % size;
  while (1) {
    if (data[h] == NULL) {
      hash_table_data_t *q =
          (hash_table_data_t *)malloc(sizeof(hash_table_data_t));
      q->key = key;
      q->val = val;
      data[h] = q;
      break;
    }
    if (data[h]->key == key) {
      if (data_delete) data_delete(data[h]->val);
      data[h]->val = val;
      break;
    }
    h = (h + 1) % size;
  }
}

static void __rehash(hash_table_t *t) {
  hash_table_data_t **new_data =
      (hash_table_data_t **)malloc(2 * t->size * sizeof(hash_table_data_t *));
  for (int i = 0; i < 2 * t->size; i++) new_data[i] = NULL;
  for (int i = 0; i < t->size; i++)
    if (t->data[i]) {
      __insert(t->data[i]->key, t->data[i]->val, 2 * t->size, new_data,
               t->data_delete);
      free(t->data[i]);
    }
  free(t->data);
  t->data = new_data;
  t->size *= 2;
}

void hash_put(hash_table_t *t, uint64_t key, void *data) {
  if (t->full >= t->size / 2) __rehash(t);
  __insert(key, data, t->size, t->data, t->data_delete);
  t->full++;
}

void *hash_get(hash_table_t *t, uint64_t key) {
  uint64_t h = __hash(key) % t->size;
  for (int i = 0; i < t->size; i++) {
    if (t->data[h] == NULL) return NULL;
    if (t->data[h]->key == key) return t->data[h]->val;
    h = (h + 1) % t->size;
  }
  return NULL;
}

void hash_remove(hash_table_t *t, uint64_t key) {
  uint64_t h = __hash(key) % t->size;
  for (int i = 0; i < t->size; i++) {
    if (t->data[h] == NULL) return;
    if (t->data[h]->key == key) {
      if (t->data_delete) (t->data_delete)(t->data[h]->val);
      free(t->data[h]);
      t->data[h] = NULL;
      return;
    }
    h = (h + 1) % t->size;
  }
}
