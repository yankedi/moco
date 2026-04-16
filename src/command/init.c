//
// Created by root on 4/3/26.
//

#include "config.h"
#include "env.h"
#include "m_exit.h"
#include "utility/mtool.h"
#include "utility/store/store.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 * 初始化moco
 */

int cmd(const char *command, const char *name);

/*
void toMkdir(char *name) {
  char *path;
  m_asprintf(&path, "%s/%s", XDG_DATA_HOME,name);
  mkdir(path, 0755);
  //if (status == -1) printf("mkdir %s fail\n", path);
  free(path);
}
*/

void init(void) {
  mkdirs(XDG_CACHE_HOME, R);
  mkdirs(XDG_CONFIG_HOME, R);
  mkdirs(XDG_DATA_HOME, R);
  mkdirs(XDG_STATE_HOME, R);
  mkdir(".moco", 0755);
  if (access(".moco/config.toml", F_OK) == 0) {
    fprintf(stderr, "config has exist\n");
    char *line = NULL;
    size_t len = 0;
    printf("overlay? [y/N]: ");
    getline(&line, &len, stdin);
    if (rpmatch(line) == 1) {
      remove(".moco/config.toml");
      FILE *fp = fopen(".moco/config.toml", "wb+");
      fprintf(fp, "[moco]\n");
      fprintf(fp, "version=\"%s\"\n", MOCO_VERSION);
      fprintf(fp, "build_date=\"%s\"\n", MOCO_BUILD_DATE);
      fclose(fp);
    }
    free(line);
  }
  m_exit(EXIT_SUCCESS);
}

int cmd(const char *command, const char *name) {
  const int ws = system(command);
  if (ws == -1)
    exit(EXIT_FAILURE);
  if (WIFEXITED(ws)) {
    return WEXITSTATUS(ws);
  } else {
    fprintf(stderr, "%s fail:%d\n", name, WTERMSIG(ws));
    exit(128 + WTERMSIG(ws));
  }
  fprintf(stderr, "command %s unknown status\n", name);
  exit(EXIT_FAILURE);
}