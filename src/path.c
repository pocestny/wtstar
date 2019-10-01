/**
 * @file path.c
 * @brief implementation of path.h
 */
#include <path.h>
#include <stdlib.h>
#include <string.h>

CONSTRUCTOR(path_item_t, const char *_val) {
  ALLOC_VAR(r, path_item_t)
  if (_val)
    r->val = strdup(_val);
  else
    r->val = NULL;
  r->prev = r->next = NULL;
  return r;
}

DESTRUCTOR(path_item_t) {
  if(!r) return;
  if (r->prev) r->prev->next = r->next;
  if (r->next) r->next->prev = r->prev;
  if (r->val) free(r->val);
  free(r);
}

path_item_t *path_item_t_clone(path_item_t *src) {
  if (!src) return NULL;
  path_item_t *r = path_item_t_new(NULL);
  if (src->val) r->val = strdup(src->val);
  return r;
}

CONSTRUCTOR(path_t, path_t *prefix, const char *suffix) {
  ALLOC_VAR(r, path_t)
  r->first = r->last = NULL;

  if (suffix) {
    char *base = strdup(suffix);
    {
      char *i = base, *j = base;
      while (1) {
        if (*i == '/' || *i == 0) {
          char c = *i;
          *i = 0;
          if (strcmp(j, ".")) {
            if (!strcmp(j, "..") && r->last) {
              path_item_t *tmp = r->last;
              if (r->first == r->last)
                r->first = r->last = NULL;
              else
                r->last = r->last->prev;
              path_item_t_delete(tmp);
            } else {
              path_item_t *pi = path_item_t_new(j);
              if (!r->first) r->first = pi;
              if (r->last) r->last->next = pi;
              pi->prev = r->last;
              r->last = pi;
            }
          }
          *i = c;
          j = i + 1;
        }
        if (*i)
          i++;
        else
          break;
      }
    }
    free(base);
  }

  if (prefix)
    for (path_item_t *it = prefix->last; it; it = it->prev) {
      path_item_t *pi = path_item_t_clone(it);
      pi->next = r->first;
      r->first = pi;
      if (!r->last) r->last = pi;
    }
  return r;
}

DESTRUCTOR(path_t) {
  if (!r) return;
  while (r->first) {
    path_item_t *it = r->first->next;
    path_item_t_delete(r->first);
    r->first = it;
  }
  free(r);
}

char * path_string(path_t *p) {
  int len=1;
  for (path_item_t *it = p->first;it;it = it->next)
    len+=1+strlen(it->val);
  char * res = (char *)malloc(len);
  res[0]=0;
  for (path_item_t *it = p->first;it;it = it->next) {
    if (it!=p->first)
      strcat(res,"/");
    strcat(res,it->val);
  }
  return res;
}
