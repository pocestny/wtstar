#ifndef __PATH_H__
#define __PATH_H__

#include <utils.h>

typedef struct _path_item_t {
  char *val;
  struct _path_item_t *prev, *next;
} path_item_t;

CONSTRUCTOR(path_item_t,const char*_val);
DESTRUCTOR(path_item_t);
path_item_t * path_item_t_clone(path_item_t *src);

typedef struct {
  path_item_t *first,*last;
} path_t;

CONSTRUCTOR(path_t, path_t *prefix, const char* suffix);
DESTRUCTOR(path_t);
char *path_string(path_t *p);


#endif
