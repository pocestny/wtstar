#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <code.h>
#include <debug.h>
#include <errors.h>
#include <reader.h>
#include <vm.h>

void error_handler(error_t *err) {
  fprintf(stderr, "%s\n", err->msg->str.base);
}

char *inf;
int EXEC_DEBUG=0;

int dump_code=0,dump_debug=0;

void print_help(int argc, char **argv) {
  printf("usage: %s [-h][-?][-D] file\n", argv[0]);
  printf("options:\n");
  printf("-h,-?         print this screen and exit\n");
  printf("-c            dump also code\n");
  printf("-g            dump debug sections if present\n");
 
  exit(0);
}

void parse_options(int argc, char **argv) {
  for (int i = 1; i < argc; i++)
    if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-?")) {
      print_help(argc, argv);
    } else
    if (!strcmp(argv[i], "-c")) {
      dump_code=1;
    } else
    if (!strcmp(argv[i], "-g")) {
      dump_debug=1;
    } else
      inf = argv[i];
}

char * mode_name(int mode) {
  static char *erew="EREW";
  static char *crew="CREW";
  static char *ccrcw="cCRCW";

  switch(mode) {
    case MEM_MODE_EREW: return erew;
    case MEM_MODE_CREW: return crew;
    case MEM_MODE_CCRCW: return ccrcw;
    default: return NULL;
  }
}

int main(int argc, char **argv) {
  inf = NULL;
  parse_options(argc, argv);
  if (!inf) print_help(argc, argv);

  register_error_handler(&error_handler);

  FILE *f = fopen(inf, "rb");
  fseek(f, 0, SEEK_END);
  int len = ftell(f);
  fseek(f, 0, SEEK_SET);
  uint8_t *in = (uint8_t *)malloc(len);
  fread(in, 1, len, f);
  fclose(f);
  
  writer_t *w = writer_t_new(WRITER_FILE);
  w->f = stdout;

  if (len<3 || in[0] != SECTION_HEADER ) {
    out_text(w,"invalid input file\n");
    exit(-1);
  }
  out_text(w,"input file:         %s\n",inf);
  out_text(w,"version byte:       %x\n",in[1]);

  if (in[1]!=1) {
    out_text(w,"version byte %x not suppoerted\n",in[1]);
    exit(-2);
  }

  virtual_machine_t *env = virtual_machine_t_new(in, len);
  free(in);

  out_text(w,"data segment:       %d B\n",env->global_size);
  out_text(w,"memory mode:        %s\n",mode_name(env->mem_mode));
  out_text(w, "input variables:\n");
  print_io_vars(w, env->n_in_vars, env->in_vars);
  out_text(w, "output variables:\n");
  print_io_vars(w, env->n_out_vars, env->out_vars);
  out_text(w, "function addresses:\n");
  for (uint32_t i = 0; i < env->fcnt; i++) {
    out_text(w, "%03d %010u (%08x)", i, env->fnmap[i], env->fnmap[i]);
    if (env->debug_info)
      out_text(w," %s",env->debug_info->fn_names[i]);
    out_text(w,"\n");
  }
  if (env->debug_info) {
    if (dump_debug) {
      for(int i=0;i<env->debug_info->n_files;i++)
        printf(">> %s\n",env->debug_info->files[i]);
      for (int i=0;i<env->debug_info->n_items;i++)
        printf("]] %d %s:%d %d - %d %d\n",i,
            env->debug_info->files[env->debug_info->items[i].fileid],
            env->debug_info->items[i].fl,
            env->debug_info->items[i].fc,
            env->debug_info->items[i].ll,
            env->debug_info->items[i].lc);

    } else {
      out_text(w,"binary file contains debug info (invoke with -g to print)\n");
    }
  } else if (dump_debug) 
    out_text(w,"no debug info present\n");

  if (dump_code) {
    out_text(w, "code:\n");
    print_code(w, env->code, env->code_size);
  }
}
