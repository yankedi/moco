//
// Created by dev on 5/2/26.
//

#include "import.h"
#include "cJSON.h"
#include "command/install/install.h"
#include "command/search/search.h"
#include "interface.h"
#include "m_exit.h"
#include "utility/file/toml.h"
#include "utility/file/zip.h"
#include "utility/mtool.h"
#include "utility/store/store.h"
#include "zip.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

static int subcommand_handler(const char *subcommand);
static int local_handler(const char *subcommand);

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
        free(buffer);
        int formatVersion = cJSON_GetObjectItemCaseSensitive(json, "formatVersion")->valueint;
        if (formatVersion != 1) {
          fprintf(stderr, "Unsupported formatVersion: %d\n", formatVersion);
          cJSON_Delete(json);
          m_exit(EX_DATAERR);
        }
        char *game = cJSON_GetObjectItemCaseSensitive(json, "game")->valuestring;
        if (strcmp(game,"minecraft") != 0) {
          fprintf(stderr, "Unsupported game: %s\n", game);
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
        if (forge)          fprintf(file, "loader = \"forge\"\nloader_version = \"%s\"\n", forge->valuestring);
        else if (neoforge)  fprintf(file, "loader = \"neoforge\"\nloader_version = \"%s\"\n", neoforge->valuestring);
        else if (fabric_loader) fprintf(file, "loader = \"fabric-loader\"\nloader_version = \"%s\"\n", fabric_loader->valuestring);
        else if (quilt_loader)  fprintf(file, "loader = \"quilt-loader\"\nloader_version = \"%s\"\n", quilt_loader->valuestring);
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
        struct zip_t *zip = zip_open(subcommands,0,'r');

        size_t total = zip_entries_total(zip);
        for (int i = 0;i < total;++i) {
          zip_entry_openbyindex(zip,i);
          const char *name_f = zip_entry_name(zip);
          if (strncmp(name_f,"overrides/",10) == 0) {
            if (!zip_entry_isdir(zip)) {
              char *path;
              m_asprintf(&path,".minecraft/%s",name_f+10);
              mkdirs(path,F);
              zip_entry_fread(zip,path);
              free(path);
            }
          }
          zip_entry_close(zip);
        }
        zip_close(zip);

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

void import_n(int argc,char *argv[]) {
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
  subcommand_handler(argv[optind]);
}

int subcommand_handler(const char *subcommand) {
  int rc = 0;
  char *str = NULL;
  char *prefix = NULL;
  char *suffix = NULL;
  SearchResult *result = NULL;
  FILE *instance_f = NULL;
  FILE *lock_f = NULL;
  toml_result_t instance = {0};
  toml_result_t lock = {0};

  if (subcommand == NULL) goto clearup;
  str = m_strdup(subcommand);
  prefix = strtok(str,":");
  suffix = strtok(NULL,":");
  if (prefix == NULL) {
    rc = local_handler(subcommand);
    goto clearup;
  }
  if (access("instance.toml", F_OK) != 0) fclose(fopen("instance.toml", "w"));
  instance_f = fopen("instance.toml","rb+");
  if (access("instance-lock.toml", F_OK) != 0) fclose(fopen("instance-lock.toml", "w"));
  lock_f = fopen("instance-lock.toml","rb+");
  instance = toml_parse_file_ex("instance.toml");
  toml_datum_t instance_r = instance.toptab;
  toml_datum_t version = toml_seek(instance_r, "game.version");
  printf("prefix: %s, suffix: %s\n", prefix, suffix);
  lock = toml_parse_file_ex("instance-lock.toml");
  toml_datum_t lock_r = lock.toptab;
  if (strcmp(prefix,"version") == 0) {
    if (version.type == TOML_STRING) {
      printf("Your game version has been declared in instance.toml:%s\n", version.u.s);
      goto clearup;
    }
    result = search_version(suffix);
    if (result == NULL) {
      fprintf(stderr, "No matching Minecraft version found for %s\n", suffix);
      goto clearup;
    }
    char *v1;
    m_asprintf(&v1,"version = \"%s\"",cJSON_GetObjectItemCaseSensitive(result->node[0],"id")->valuestring);
    toml_add_on_table(instance_f,"[game]",v1);
    free(v1);
    fflush(instance_f);
    installVersion_n();
  }else if (strcmp(prefix,"loader") == 0) {
    if (suffix == NULL) {
      printf("Suffix is NULL\n");
      goto clearup;
    }
    if (strcmp(suffix,"forge") == 0) {
      result = search_forge(version.u.s,V);
    }else if (strcmp(suffix,"neoforge") == 0) {
      result = search_neoforge(version.u.s,V);
    }else if (strcmp(suffix,"fabric") == 0) {
      result = search_fabric(NULL);
    }else {
      fprintf(stderr, "Unknown or unsupported loader: %s\n", suffix);
      goto clearup;
    }
    if (result == NULL) {
      fprintf(stderr, "No matching loader version found for Minecraft %s\n", version.u.s);
      goto clearup;
    }
    char *v1,*v2;
    m_asprintf(&v1,"loader = \"%s\"",cJSON_GetObjectItemCaseSensitive(result->node[0],"loader")->valuestring);
    m_asprintf(&v2,"loader_version = \"%s\"",cJSON_GetObjectItemCaseSensitive(result->node[0],"loader_version")->valuestring);
    toml_add_on_table(instance_f,"[dependencies]",v1);
    toml_add_on_table(instance_f,"[dependencies]",v2);
    free(v1);
    free(v2);
    fflush(instance_f);
    installDependencies();
  }else if (strcmp(prefix,"mod") == 0) {
    result = search_mod(suffix);
  }else if (strcmp(prefix,"modpack") == 0) {
    result = search_modpack(suffix);
  }else if (strcmp(prefix,"shader") == 0) {
    result = search_shader(suffix);
  }else {
    printf("Unknown prefix: %s\n", prefix);
    goto clearup;
  }
  clearup:
  if (lock_f) fclose(lock_f);
  toml_free(lock);
  if (instance_f) fclose(instance_f);
  toml_free(instance);
  free_SearchResult(result);
  free(str);
  return rc;
}

int local_handler(const char *subcommand) {

}