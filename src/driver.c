#include "driver.h"
#include "parser.h"
#include "scanner.h"

#include <string.h>


static void driver_error_handler(const char *s,...) {
  va_list args;
  va_start(args,s);
  fprintf(stderr, "driver error, ");
  vfprintf(stderr,s,args);
  fprintf(stderr,"\n");
  va_end(args);
}

typedef struct _include_file_t{
  char *name;
  char *content;
  FILE *f;
  YY_BUFFER_STATE buf;
  int lineno,col,included;
  struct _include_file_t *next, *included_from;
} include_file_t;

static CONSTRUCTOR(include_file_t) {
  ALLOC_VAR(r,include_file_t)
  r->name=NULL;
  r->content = NULL;
  r->f = NULL;
  r->buf = NULL;
  r->lineno=0;
  r->col=0;
  r->next=NULL;
  r->included_from=NULL;
  return r;
}

static DESTRUCTOR(include_file_t) {
  if (r==NULL) return;
  if (r->name) free(r->name);
  if (r->content) free(r->content);
  if (r->f) fclose(r->f);
  if (r->buf) yy_delete_buffer(r->buf);
  if (r->next) include_file_t_delete(r->next);
  free(r);
}


include_file_t *files,*current;



/* insert a new empty file into hashtable */
static include_file_t *insert_file(const char *filename) {
  ALLOC_VAR(file,include_file_t)
  if (files) file->next=files;
  files=file;
  file->name = strdup(filename);
  return file;
}


/* initialize the driver */
void driver_init() {
  files=current=NULL;
}

/* preload / unload the given file */
void driver_set_file(const char *filename, const char *content) {
  
  include_file_t *file;
  for(file=files;strcmp(file->name,filename);file=file->next);

  if (file && (file->f || file->buf)) {
    driver_error_handler("declined to preload current file, skipping");
    return;
  }

  if (file == NULL)
    file = insert_file(filename);
  else if (file->content)
    free(file->content);
  if (content) {
    file->content=strdup(content);
  } else
    file->content = NULL;
}

/* main parsing function */
ast_t *driver_parse(const char *filename) {
  ast_t * ast = ast_t_new();

  driver_push_file(filename);
  if (driver_current_file()) yyparse(ast);
  return ast;
}

/* switch to a new file */
void driver_push_file(const char *filename) {  
  
  include_file_t *file;
  for(file=files;file&&strcmp(file->name,filename);file=file->next);
  
  if (file == NULL) file = insert_file(filename);
  
  if (file->buf) {
    driver_error_handler("circular includes not allowed (%s)",filename);
    return;
  }

  if (file->content) {
    file->buf = yy_scan_string(file->content);
  } else {
    file->f = fopen(filename, "r");
    if (file->f) {
      file->buf = yy_create_buffer(file->f, YY_BUF_SIZE);
      yy_switch_to_buffer(file->buf);
    }
    else {
      driver_error_handler("cannot open file %s",filename);
      return;
    }
  }

  file->included_from=current;
  current=file;
}

int driver_pop_file() {
  
  include_file_t *oldfile = current;
  include_file_t *newfile = current->included_from;
  current=newfile;
  if (oldfile) oldfile->included_from=NULL;

  if (oldfile->f) {
    fclose(oldfile->f);
    oldfile->f = NULL;
  } 

  if (newfile) yy_switch_to_buffer(newfile->buf);
  
  yy_delete_buffer(oldfile->buf);
  oldfile->buf=NULL;

  if (newfile == NULL) return 0;
  else return 1;
  
}

void driver_destroy() {
  include_file_t_delete(files);
}

const char * driver_current_file() {
  if (current==NULL) return NULL;
  return current->name;
}

int driver_current_line() {
  if (current==NULL) return -1;
  return current->lineno;
}

int driver_current_column() {
  if (current==NULL) return -1;
  return current->col;
}

void driver_set_current_pos(int l,int col) {
  if (current) {
    current->lineno=l;
    current->col=col;
  }
}
