//
// Created by root on 4/5/26.
//

#include "store.h"
#include "env.h"
#include "utility/mtool.h"

#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

int toStore() { return 0; }

char *get_store_path(const char *sha1) {
  char *path;
  m_asprintf(&path, "%s/store/v1/%02.2s/%s", XDG_DATA_HOME, sha1, sha1);
  mkdirs(path, F);
  return path;
}
char *get_tmp(const char *store) {
  char *path;
  m_asprintf(&path, "%s.tmp", store);
  mkdirs(path, F);
  return path;
}
/*
char *getPath(Package *p) {
  char *path;
  char *str;
  FILE *fp =NULL;
  m_asprintf(&str,"store/v1/%c%c",p->sha1[0],p->sha1[1]);
  toMkdir(str);
  free(str);
  m_asprintf(&path, "%s/store/v1/%c%c/%s",
XDG_DATA_HOME,p->sha1[0],p->sha1[1],p->sha1); if ((fp = fopen(path,"rb")) !=
NULL) { if ((fp=fopen(p->path,"rb")) == NULL) link_mkdir(path,p->path);
    free(path);
    if (fp) fclose(fp);
    return NULL;
  }
  if (fp) fclose(fp);
  return path;
}
*/

void link_mkdir(const char *store, const char *path) {
  mkdirs(path, F);
  if (link(store, path) == -1) {
    if (errno != EEXIST)
      perror("link");
  } else {
    // printf("\033[32mLink successful: \"%s\"\033[0m\n", path);
  }
  // TODO 错误处理
}

void mkdirs(const char *path, mkdir_mode mode) {
  char *path_c = m_strdup(path);
  if (mode == F)
    path_c = dirname(path_c);
  for (char *p = path_c; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      mkdir(path_c, 0755);
      *p = '/';
    }
  }
  mkdir(path_c, 0755);
  free(path_c);
}

/**
 *
 * @param name 传入maven格式
 * @return 返回一个栈字符串
 */
char *reMaven(const char *name) {
  char *tmp = m_strdup(name);
  char *token = strtok(tmp,":");
  char *group_id = token;
  token = strtok(NULL,":");
  const char *artifact_id = token;
  token = strtok(NULL,":");
  const char *version = token;
  char *p = group_id;
  while (*p != '\0') {
    if (*p == '.') *p = '/';
    ++p;
  }
  char *path;
  m_asprintf(&path,"%s/%s/%s/%s-%s.jar",group_id,artifact_id,version,artifact_id,version);
  free(tmp);
  return path;
}