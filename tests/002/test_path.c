#include <path.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
  path_t *p1 = path_t_new(NULL,"./hnoj/../tchor/blbecek/../kuauicek");
  path_t *p2 = path_t_new(p1,"smrduty/syr/../../spotena/vevericka.clup");
  char *s = path_string(p2);
  printf("%s\n",s);
  free(s);
}
