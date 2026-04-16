//
// Created by root on 4/10/26.
//

#include "start.h"
#include "cjson/cJSON.h"
#include "command/install/install.h"
#include "config.h"
#include "interface.h"
#include "m_exit.h"
#include "tomlc17.h"
#include "utility/file/json.h"
#include "utility/mtool.h"

#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <sys/wait.h>

static toml_result_t result;
static toml_result_t account_r;
static toml_result_t profile_r;
static toml_result_t instance_r;

cJSON *version_json;

char *java_path;
char *game;
char *jvm;
char *mainClass;
char *command;

// char *jvm_a;
char *jvm_a_Xms;
char *jvm_a_Xmx;

m_string jvm_a[] = {{"${natives_directory}", NULL},
                    {"${launcher_name}", NULL},
                    {"${launcher_version}", NULL},
                    {"${classpath}", NULL}};

m_string game_a[] = {
    {"${auth_player_name}", NULL},  {"${version_name}", NULL},
    {"${game_directory}", NULL},    {"${assets_root}", NULL},
    {"${assets_index_name}", NULL}, {"${auth_uuid}", NULL},
    {"${auth_access_token}", NULL}, {"${clientid}", NULL},
    {"${auth_xuid}", NULL},         {"${version_type}", NULL},
};

static int analyze(void);

static void to_free(void) {
  free(java_path);
  free(command);
  free(game);
  free(jvm);
  free(mainClass);
  java_path = NULL;
  command = NULL;
  game = NULL;
  jvm = NULL;
  mainClass = NULL;
  for (int i = 0; i < sizeof(jvm_a) / sizeof(m_string); ++i) {
    free(jvm_a[i].value);
    jvm_a[i].value = NULL;
  }
  for (int i = 0; i < sizeof(game_a) / sizeof(m_string); ++i) {
    free(game_a[i].value);
    game_a[i].value = NULL;
  }
  toml_free(instance_r);
  toml_free(profile_r);
  toml_free(account_r);
  cJSON_Delete(version_json);
}
void start(void) {
  jvm = m_strdup("");
  game = m_strdup("");
  if (analyze() == 0) {
    printf("Starting Minecraft %s...\n", game_a[1].value);
    m_asprintf(&command, "%s %s %s %s", java_path, jvm, mainClass, game);
    //printf("%s\n", command);
    setenv("__NV_PRIME_RENDER_OFFLOAD", "1", 1);
    setenv("__GLX_VENDOR_LIBRARY_NAME", "nvidia", 1);

    int max_args = 1024;
    char **args = malloc(max_args * sizeof(char *));
    int arg_count = 0;

    args[arg_count++] = java_path;

    strtok(command, " ");
    char *token = strtok(NULL, " ");
    while (token != NULL && arg_count < max_args - 1) {
      args[arg_count++] = token;
      token = strtok(NULL, " ");
    }
    args[arg_count] = NULL;
    pid_t pid = fork();
    if (pid == -1) {
      // fork 失败
      perror("fork failed");
      to_free();
      free(args);
      m_exit(EX_OSERR);
    } else if (pid == 0) {
      // ================== 这里是子进程 ==================
      if (execvp(java_path, args) == -1) {
        perror("execvp failed");
        exit(EXIT_FAILURE);
      }
    } else {
      // ================== 这里是父进程 (启动器) ==================
#if 0
      int status;
      waitpid(pid, &status, 0);

      if (WIFEXITED(status)) {
        printf("Minecraft exited with status: %d\n", WEXITSTATUS(status));
      }
#endif

      to_free();
      free(args);
    }
  } else {
    to_free();
    m_exit(EX_DATAERR);
  }
}

static int analyze(void) {
  if (access("instance.toml", F_OK) == 0) {
    if (access(".minecraft/versions/version.json", F_OK) == 0) {
      if (access(".moco/account.toml", F_OK) == 0) {
        jvm_a[0].value = m_strdup(".moco/natives");
        jvm_a[1].value = m_strdup("moco");
        jvm_a[2].value = m_strdup(MOCO_VERSION);
        jvm_a[3].value = m_strdup("");
        game_a[2].value = m_strdup(".minecraft");
        game_a[3].value = m_strdup(".minecraft/assets");
        game_a[7].value = m_strdup("");

        profile_r = toml_parse_file_ex(".moco/profile.toml");
        toml_datum_t profile_root = profile_r.toptab;
        game_a[0].value = m_strdup(toml_seek(profile_root, "profile.name").u.s);
        version_json = file_to_json(".minecraft/versions/version.json");
        game_a[1].value = m_strdup(
            cJSON_GetObjectItemCaseSensitive(version_json, "id")->valuestring);
        game_a[4].value =
            m_strdup(cJSON_GetObjectItemCaseSensitive(version_json, "assets")
                         ->valuestring);
        mainClass =
            m_strdup(cJSON_GetObjectItemCaseSensitive(version_json, "mainClass")
                         ->valuestring);
        game_a[5].value = m_strdup(toml_seek(profile_root, "profile.id").u.s);
        account_r = toml_parse_file_ex(".moco/account.toml");
        toml_datum_t account_root = account_r.toptab;
        game_a[6].value =
            m_strdup(toml_seek(account_root, "account.minecraft_token").u.s);
        game_a[8].value = m_strdup(toml_seek(account_root, "account.uhs").u.s);
        game_a[9].value =
            m_strdup(cJSON_GetObjectItemCaseSensitive(version_json, "type")
                         ->valuestring);
        cJSON *version_arguments_json =
            cJSON_GetObjectItemCaseSensitive(version_json, "arguments");
        cJSON *version_arguments_game_json =
            cJSON_GetObjectItemCaseSensitive(version_arguments_json, "game");
        // game
        cJSON *item;
        char *pre, *tmp, flag;
        cJSON_ArrayForEach(item, version_arguments_game_json) {
          if (item->type == cJSON_String) {
            flag = 1;
            for (int i = 0; i < sizeof(game_a) / sizeof(m_string); ++i) {
              if (strcmp(item->valuestring, game_a[i].key) == 0) {
                pre = game;
                m_asprintf(&tmp, "%s %s", pre, game_a[i].value);
                free(pre);
                game = tmp;
                flag = 0;
                break;
              }
            }
            if (flag) {
              pre = game;
              m_asprintf(&tmp, "%s %s", pre, item->valuestring);
              free(pre);
              game = tmp;
            }
          }
        }
        cJSON *version_arguments_jvm_json =
            cJSON_GetObjectItemCaseSensitive(version_arguments_json, "jvm");
        cJSON *libraries_res =
            cJSON_GetObjectItemCaseSensitive(version_json, "libraries");
        // classpath
        cJSON_ArrayForEach(item, libraries_res) {
          flag = 1;
          cJSON *rules_res = cJSON_GetObjectItemCaseSensitive(item, "rules");
          cJSON *rule_res = cJSON_GetArrayItem(rules_res, 0);
          cJSON *os_res = cJSON_GetObjectItemCaseSensitive(rule_res, "os");
          cJSON *os_name_res = cJSON_GetObjectItemCaseSensitive(os_res, "name");
          if (os_name_res)
            if (strcmp(os_name_res->valuestring, "linux") != 0)
              flag = 0;
          if (flag) {
            cJSON *downloads_res =
                cJSON_GetObjectItemCaseSensitive(item, "downloads");
            cJSON *artifact_res =
                cJSON_GetObjectItemCaseSensitive(downloads_res, "artifact");
            cJSON *path_res =
                cJSON_GetObjectItemCaseSensitive(artifact_res, "path");
            pre = jvm_a[3].value;
            m_asprintf(&tmp, "%s:.minecraft/libraries/%s", jvm_a[3].value,
                       path_res->valuestring);
            free(pre);
            jvm_a[3].value = tmp;
            tmp = NULL;
          }
        }
        pre = jvm_a[3].value;
        m_asprintf(&tmp, "%s:.minecraft/versions/version/version.jar",
                   jvm_a[3].value);
        free(pre);
        jvm_a[3].value = tmp;
        tmp = NULL;
        // jvm
        cJSON_ArrayForEach(item, version_arguments_jvm_json) {
          if (item->type == cJSON_String) {
            char *resolved = m_strdup(item->valuestring);
            for (int i = 0; i < sizeof(jvm_a) / sizeof(m_string); ++i) {
              char *next = m_replace(resolved, jvm_a[i]);
              if (next != NULL) {
                free(resolved);
                resolved = next;
              }
            }

            pre = jvm;
            m_asprintf(&tmp, "%s %s", pre, resolved);
            free(pre);
            free(resolved);
            jvm = tmp;
          }
        }
        // java_path
        instance_r = toml_parse_file_ex("instance.toml");
        toml_datum_t instance_root = instance_r.toptab;
        toml_datum_t java_path_res =
            toml_seek(instance_root, "launch.java_path");
        if (java_path_res.type != TOML_UNKNOWN) {
          java_path = m_strdup(java_path_res.u.s);
        } else {
          if (access(".moco/java/bin/java", F_OK) == 0 ||
              download_java() == 0) {
            java_path = m_strdup(".moco/java/bin/java");
          } else {
            return 1;
          }
        }
      } else {
        fprintf(stderr, "Error: .moco/account.toml not found.\n"
                        "Please run 'moco login' first.\n");
        return 1;
      }
    } else {
      fprintf(stderr, "Error: version.json not found.\n"
                      "Please run 'moco install' first.\n");
      return 1;
    }
  } else {
    fprintf(stderr, "Error opening instance.toml: %s\n", result.errmsg);
    return 1;
  }
  return 0;
}