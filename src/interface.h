//
// Created by root on 4/3/26.
//
#include "cJSON.h"

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

typedef struct {
  cJSON **node;
  char count;
} SearchResult;

extern void free_package(package *p);
extern void free_SearchResult(SearchResult *result);
#endif // MOCO_OPT_H
