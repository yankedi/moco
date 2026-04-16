//
// Created by root on 4/3/26.
//

#include "main.h"
#include "command/init.h"
#include "command/install/install.h"
#include "command/login/login.h"
#include "command/search/search.h"
#include "command/start/start.h"
#include "command/update/update.h"
#include "env.h"
#include "interface.h"
#include "m_exit.h"
#include "utility/download/m_epoll.h"

#include <curl/curl.h>

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
  env();
  curl_global_init(CURL_GLOBAL_ALL);
  init_epoll_download_task();
  int opt;
  int option_index = 0;
  while ((opt = getopt_long(argc, argv, "+h", cli_options, &option_index)) !=
         -1) {
    switch (opt) {
    case 'h':
      printf("Usage: moco [options]\n");
      printf("Options:\n");
      printf("  -h, --help    Show this help message\n");
      break;
    default:
      fprintf(stderr, "Unknown option: %c\n", opt);
    }
  }
  char *subcommand = argv[optind];

  if (subcommand != NULL) {
    if (strcmp(subcommand, "init") == 0)
      init();
    if (strcmp(subcommand, "login") == 0)
      login();
    if (strcmp(subcommand, "install") == 0)
      install();
    if (strcmp(subcommand, "update") == 0)
      update();
    if (strcmp(subcommand, "search") == 0) {
      search(argc - optind, argv + optind);
    }
    if (strcmp(subcommand, "start") == 0) {
      start();
    }
  }
  m_exit(EXIT_SUCCESS);
}
