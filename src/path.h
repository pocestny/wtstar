/**
 * @file path.h
 * @brief Parser for path strings
 */
#ifndef __PATH_H__
#define __PATH_H__

#include <utils.h>

//! one entry of path
typedef struct _path_item_t {
  char *val;                        //!< name
  struct _path_item_t *prev,        //!< preious entry  
                      *next;        //!< next entry
} path_item_t;

//! creates a new path_item_t object with a given name and NULL pointers
CONSTRUCTOR(path_item_t,const char*_val);

//! deallocates memory. If the item was part of a list, it is remove from the list first
DESTRUCTOR(path_item_t);

//! deep copy (clone `val`)
path_item_t * path_item_t_clone(path_item_t *src);

//! the path is bi-directional list of path items
typedef struct {
  path_item_t *first,*last;
} path_t;

/** 
 * @brief path_t constructor
 *
 * creates a path by cloning the prefix, and adding the items from the suffix string; 
 * `.` and `..` entries are resolved
 */
CONSTRUCTOR(path_t, path_t *prefix, const char* suffix);

//! destroys the linked list and deallocates memory
DESTRUCTOR(path_t);

//! get the path as string
char *path_string(path_t *p);


#endif
