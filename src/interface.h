//
// Created by root on 4/3/26.
//
#include <getopt.h>

#ifndef MOCO_OPT_H
#define MOCO_OPT_H
extern struct option cli_options[];
extern struct option search_options[];
typedef struct {
  const char *const key;
  char *value;
} m_string;
typedef struct {
  char *url;
  char *sha1;
  char *path;
  char *store;
} package;

typedef struct {
  package *pack;
  int front;
  int back;
} OnePackages;

typedef struct {
  int success;
  int fail;
} t_result;

extern void free_package(package *p);
#endif // MOCO_OPT_H
