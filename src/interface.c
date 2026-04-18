//
// Created by root on 4/3/26.
//

#include "interface.h"
#include <getopt.h>
#include <stddef.h>
#include <stdlib.h>

struct option cli_options[] = {{"help", no_argument, NULL, 'h'}, {0, 0, 0, 0}};

struct option search_options[] = {{"help", no_argument, NULL, 'h'},
                                  {"version", required_argument, NULL, 'v'},
                                  {0, 0, 0, 0}};

void free_package(package *p) {
  if (p) {
    if (p->path)
      free(p->path);
    if (p->sha1)
      free(p->sha1);
    if (p->url)
      free(p->url);
    if (p->store)
      free(p->store);
    free(p);
  }
}

void free_SearchResult(SearchResult *result) {
  for (int i = 0;i < result->count;++i) {
    cJSON_Delete(result->node[i]);
  }
  free(result->node);
  free(result);
}
