#include <string.h>

#include <driver.h>
#include <errors.h>
#include <path.h>

/**
 * @brief Invoke an error.
 * Called from #driver_set_file and #driver_push_file when someting went wrong.
 * Create an instance of \ref error_t (see errors.h) and emits it.
 * In the `wtc` cli tool it results in printing the message to `stderr`, the
 * web module displays error in a panel.
 */
static void driver_error_handler(const char *s, ...) {
  error_t *err = error_t_new();
  append_error_msg(err, "driver error: ");
  va_list args;
  int n;
  get_printed_length(s, n);
  va_start(args, s);
  append_error_vmsg(err, n, s, args);
  va_end(args);
  emit_error(err);
}

CONSTRUCTOR(include_file_t, const char *name) {
  ALLOC_VAR(r, include_file_t)
  r->f = NULL;
  r->buf = NULL;
  r->lineno = 1;
  r->col = 1;
  r->next = NULL;
  r->included_from = NULL;
  r->name = strdup(name);
  r->next = NULL;
  r->content = NULL;
  r->included = 0;
  r->scanner = NULL; //TODO remove scanner dependency
  return r;
}

DESTRUCTOR(include_file_t) {
  if (r == NULL) return;
  if (r->name) free(r->name);
  if (r->content) free(r->content);
  if (r->f) fclose(r->f);
  if (r->buf) yy_delete_buffer(r->buf, r->scanner); //TODO remove scanner dependency
  if (r->next) include_file_t_delete(r->next);
  free(r);
}

// TODO remove global var
// static include_file_t *files=NULL,  //!< the list of known included files
//     *current=NULL;                  //!< pointer to the list of included files

CONSTRUCTOR(include_project_t) {
  ALLOC_VAR(r, include_project_t);
  r->files = NULL;
  r->current = NULL;
  return r;
}

DESTRUCTOR(include_project_t) {
  if(r->files) include_file_t_delete(r->files);
  r->current = NULL;
}

// TODO remove global var
writer_t *driver_error_writer = NULL;

//! internal: Insert a new empty file
static include_file_t *insert_file(include_project_t *ip, const char *filename) {
  include_file_t *file = include_file_t_new(filename);
  if (ip->files) file->next = ip->files;
  ip->files = file;
  return file;
}

/* initialize the driver */
void driver_init() {  /*files = current = NULL;*/ }

/* preload / unload the given file */
void driver_set_file(include_project_t *ip, const char *filename, const char *content) {
  include_file_t *file;
  for (file = ip->files; file && strcmp(file->name, filename); file = file->next)
    ;
  if (file && (file->f || file->buf)) {
    driver_error_handler("declined to preload current file, skipping");
    return;
  }
  if (file == NULL)
    file = insert_file(ip, filename);
  else if (file->content)
    free(file->content);
  if (content) {
    file->content = strdup(content);
  } else
    file->content = NULL;
}

//! use the \ref path_t from path.h to resolve `.` and `..`, and add relative
//! path
static char *normalize_filename(include_file_t *prefix, const char *f) {
  path_t *p;
  if (prefix)
    p = path_t_new(NULL, prefix->name);
  else
    p = NULL;
  if (p && p->last) {
    path_item_t *it = p->last;
    p->last = p->last->prev;
    if (!p->last) p->first = NULL;
    path_item_t_delete(it);
  }
  path_t *np = path_t_new(p, f);
  char *result = path_string(np);
  path_t_delete(np);
  path_t_delete(p);
  return result;
}

/* main parsing function */
ast_t *driver_parse(const char *filename) {
  include_project_t *ip;
  ast_t *ast = ast_t_new();

  // struct yyextra_t * extra;
  //TODO! extra lineno = 1
  yyscan_t scanner;       
  yylex_init(&scanner);
  // yyset_lineno(1, scanner);
  // yyset_column(1, scanner);
  // TODO!

  for (include_file_t *file = ip->files; file; file = file->next)
    file->included = 0;

  char *name = normalize_filename(NULL, filename);
  driver_push_file(ip, name, 1, scanner);
  free(name);

  if (driver_current_file(ip->current)) yyparse(ast, scanner);   
  yylex_destroy(scanner);   
  return ast;
}

/* switch to a new file */
void driver_push_file(include_project_t *ip, const char *filename, int only_once, yyscan_t scanner) {
  char *name = normalize_filename(ip->current, filename);

  include_file_t *file;
  for (file = ip->files; file && strcmp(file->name, name); file = file->next)
    ;

  if (file == NULL) file = insert_file(ip, name);
  if (file->included && only_once) {
    free(name);
    return;
  }
  file->included = 1;
  file->scanner = scanner;

  if (file->buf) {
    driver_error_handler("circular includes not allowed (%s)", name);
    free(name);
    return;
  }

  if (file->content) {
    file->buf = yy_scan_string(file->content, scanner);
  } else {
    file->f = fopen(name, "r");
    if (file->f) {
      file->buf = yy_create_buffer(file->f, YY_BUF_SIZE, scanner);
      yy_switch_to_buffer(file->buf, scanner);
    } else {
      driver_error_handler("cannot open file %s", name);
      free(name);
      return;
    }
  }

  file->included_from = ip->current;
  ip->current = file;
  free(name);
}

/* remove file from stack */
int driver_pop_file(include_project_t *ip) {
  include_file_t *oldfile = ip->current;
  include_file_t *newfile = ip->current->included_from;
  ip->current = newfile;
  if (oldfile) oldfile->included_from = NULL;

  if (oldfile->f) {
    fclose(oldfile->f);
    oldfile->f = NULL;
  }

  if (newfile) yy_switch_to_buffer(newfile->buf, newfile->scanner);

  yy_delete_buffer(oldfile->buf, oldfile->scanner);
  oldfile->buf = NULL;

  if (newfile == NULL)
    return 0;
  else
    return 1;
}

/* deallocat memory */
void driver_destroy() { /*include_file_t_delete();*/ }

/* *********************** */
/* various getters/setters */
const char *driver_current_file(include_file_t *current) {
  if (current == NULL) return NULL;
  return current->name;
}

int driver_current_line(include_file_t *current) {
  if (current == NULL) return -1;
  return current->lineno;
}

int driver_current_column(include_file_t *current) {
  if (current == NULL) return -1;
  return current->col;
}

void driver_set_current_pos(include_file_t *current, int l, int col) {
  if (current) {
    current->lineno = l;
    current->col = col;
  }
}
