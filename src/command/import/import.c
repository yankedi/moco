//
// Created by dev on 5/2/26.
//

#include "import.h"
#include "cJSON.h"
#include "interface.h"
#include "m_exit.h"
#include "utility/file/zip.h"
#include "utility/mtool.h"
#include "utility/store/store.h"

#include <bits/getopt_core.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <zip.h>

void import(int argc, char *argv[]) {
  int opt;
  int option_index = 0;
  optind = 1;
  while ((opt = getopt_long(argc,argv,"+h",import_options,&option_index)) != -1) {
    switch (opt) {
      case 'h':
        printf("Usage: moco import archive.zip/archive.mrpack/... [options]\n");
        printf("Options:\n");
        printf("  -h, --help    Show this help message\n");
        break;
      default:
       fprintf(stderr, "Unknown option `%c'\n", opt);
    }
  }
  char *subcommands = argv[optind];
  if (access(subcommands, F_OK) == 0) {
    printf("Importing from %s\n", subcommands);
    const char *p = subcommands+strlen(subcommands)-1;
    char flag = 1;
    while (p != subcommands) {
      if (*(p-1) == '.') {
        flag = 0;
        break;
      }
      --p;
    }
    if (flag == 0) {
      printf("suffix is：");
      puts(p);
      if (strcmp(p,"zip") == 0) {

      }else if (strcmp(p,"mrpack") == 0) {
        char *buffer = zip_get_file(subcommands, "modrinth.index.json");
        cJSON *json = cJSON_Parse(buffer);
        int formatVersion = cJSON_GetObjectItemCaseSensitive(json, "formatVersion")->valueint;
        if (formatVersion != 1) {
          fprintf(stderr, "Unsupported formatVersion: %d\n", formatVersion);
          free(buffer);
          cJSON_Delete(json);
          m_exit(EX_DATAERR);
        }
        char *game = cJSON_GetObjectItemCaseSensitive(json, "game")->valuestring;
        if (strcmp(game,"minecraft") != 0) {
          fprintf(stderr, "Unsupported game: %s\n", game);
          free(buffer);
          cJSON_Delete(json);
          m_exit(EX_DATAERR);
        }
        if (access("instance.toml",F_OK) == 0) {
          char *line = NULL;
          size_t len = 0;
          printf("instance.toml already exists,Override? [y/N]: ");
          getline(&line, &len, stdin);
          if (rpmatch(line) == 1) {
            remove("instance.toml");
          }else {
            free(line);
            free(buffer);
            cJSON_Delete(json);
            m_exit(EXIT_SUCCESS);
          }
          free(line);
        }
        char *name = cJSON_GetObjectItemCaseSensitive(json, "name")->valuestring;
        char *versionId = cJSON_GetObjectItemCaseSensitive(json, "versionId")->valuestring;
        cJSON *dependencies = cJSON_GetObjectItemCaseSensitive(json, "dependencies");
        char *minecraft_version = cJSON_GetObjectItemCaseSensitive(dependencies,"minecraft")->valuestring;
        cJSON *forge = cJSON_GetObjectItemCaseSensitive(dependencies,"forge");
        cJSON *neoforge = cJSON_GetObjectItemCaseSensitive(dependencies,"neoforge");
        cJSON *fabric_loader = cJSON_GetObjectItemCaseSensitive(dependencies,"fabric-loader");
        cJSON *quilt_loader = cJSON_GetObjectItemCaseSensitive(dependencies,"quilt-loader");
        FILE *file = fopen("instance.toml","wb");
        fprintf(file, "[game]\n"
                      "name = \"%s\"\n"
                      "modpack_version = \"%s\"\n"
                      "modpack_mode = \"modrinth\"\n"
                      "version = \"%s\"\n", name,versionId,minecraft_version);
        fprintf(file, "[dependencies]\n");
        if (forge) fprintf(file, "forge = \"%s\"\n", forge->valuestring);
        if (neoforge) fprintf(file, "neoforge = \"%s\"\n", neoforge->valuestring);
        if (fabric_loader) fprintf(file, "fabric-loader = \"%s\"\n", fabric_loader->valuestring);
        if (quilt_loader) fprintf(file, "quilt-loader = \"%s\"\n", quilt_loader->valuestring);
        cJSON *mods = cJSON_GetObjectItemCaseSensitive(json,"files");
        cJSON *item;
        cJSON_ArrayForEach(item,mods) {
          cJSON *env = cJSON_GetObjectItem(item,"env");
          char *env_client = cJSON_GetObjectItem(env,"client")->valuestring;
          if (strcmp(env_client,"unsupported") != 0) {
            char *path = cJSON_GetObjectItemCaseSensitive(item,"path")->valuestring;
            m_asprintf(&path, ".minecraft/%s", path);
            char *sha1 = cJSON_GetObjectItemCaseSensitive(
              cJSON_GetObjectItemCaseSensitive(item,"hashes"),"sha1")->valuestring;
            char *url = cJSON_GetArrayItem(
              cJSON_GetObjectItemCaseSensitive(item,"downloads"),0)->valuestring;
            if (path&&sha1&&url) {
              fprintf(file, "[[mods]]\n"
                            "url = \"%s\"\n"
                            "sha1 = \"%s\"\n"
                            "path = \"%s\"\n",url,sha1,path);
            }
            free(path);
          }
        }
        fclose(file);
        cJSON_Delete(json);
      }else fprintf(stderr,"Unsupported file suffix: %s\n", p);
    }else {
      fprintf(stderr,"File %s has no suffix\n", subcommands);
    }
  }else {
    fprintf(stderr,"File %s does not exist\n", subcommands);
  }
}