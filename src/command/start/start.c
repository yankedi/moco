//
// Created by root on 4/10/26.
//

#include "start.h"
#include "cJSON.h"
#include "command/install/install.h"
#include "command/login/oauth2.h"
#include "config.h"
#include "interface.h"
#include "m_exit.h"
#include "tomlc17.h"
#include "utility/file/json.h"
#include "utility/mtool.h"
#include "utility/store/store.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

static toml_result_t result;
static toml_result_t account_r;
static toml_result_t profile_r;
static toml_result_t instance_r;

cJSON *version_json;

char *java_path;
char *mainClass;

struct start_args {
  char **value;
  int count;
};

static void add_arg(struct start_args *args, const char *arg) {
  args->count++;
  args->value = realloc(args->value, args->count * sizeof(char *));
  if (!args->value) { perror("realloc"); exit(EX_OSERR); }
  args->value[args->count - 1] = m_strdup(arg);
}

static struct start_args jvm = {NULL, 0};
static struct start_args game = {NULL, 0};
static struct start_args duj = {NULL, 0};

char *jvm_a_Xms;
char *jvm_a_Xmx;

m_string jvm_a[] = {
  {"${natives_directory}", NULL},
  {"${launcher_name}", NULL},
  {"${launcher_version}", NULL},
  {"${classpath}", NULL}
};

m_string game_a[] = {
  {"${auth_player_name}", NULL},
  {"${version_name}", NULL},
  {"${game_directory}", NULL},
  {"${assets_root}", NULL},
  {"${assets_index_name}", NULL},
  {"${auth_uuid}", NULL},
  {"${auth_access_token}", NULL},
  {"${clientid}", NULL},
  {"${auth_xuid}", NULL},
  {"${version_type}", NULL},
  {"${user_type}", NULL}
};

static int analyze(void);

static void to_free(void) {
  free(java_path);
  for (int i = 0; i < jvm.count; i++) free(jvm.value[i]);
  free(jvm.value);
  for (int i = 0; i < game.count; i++) free(game.value[i]);
  free(game.value);
  for (int i = 0; i < duj.count; i++) free(duj.value[i]);
  free(duj.value);
  free(mainClass);
  for (int i = 0; i < sizeof(jvm_a) / sizeof(m_string); ++i) {
    free(jvm_a[i].value);
    jvm_a[i].value = NULL;
  }
  for (int i = 0; i < sizeof(game_a) / sizeof(m_string); ++i) {
    free(game_a[i].value);
    game_a[i].value = NULL;
  }
  free(jvm_a_Xms);
  free(jvm_a_Xmx);
  toml_free(instance_r);
  toml_free(profile_r);
  toml_free(account_r);
  cJSON_Delete(version_json);
}
void start(void) {
  if (analyze() == 0) {
    printf("Starting Minecraft %s...\n", game_a[1].value);
    if (duj.count > 0) {
      int new_count = jvm.count + duj.count;
      jvm.value = realloc(jvm.value, new_count * sizeof(char *));
      if (!jvm.value) { perror("realloc"); exit(EX_OSERR); }
      memmove(jvm.value + duj.count, jvm.value, jvm.count * sizeof(char *));
      for (int i = 0; i < duj.count; i++)
        jvm.value[i] = duj.value[i];
      jvm.count = new_count;
      free(duj.value);
      duj.value = NULL;
      duj.count = 0;
    }
    int total = 1 + jvm.count + 1 + game.count;
    char **args = malloc((total + 1) * sizeof(char *));
    int arg_count = 0;
    args[arg_count++] = java_path;
    for (int i = 0; i < jvm.count; i++) args[arg_count++] = jvm.value[i];
    args[arg_count++] = mainClass;
    for (int i = 0; i < game.count; i++) args[arg_count++] = game.value[i];
    args[arg_count] = NULL;
    printf("start command:\n");
    for (int i = 0; i < arg_count; i++) printf("  [%d] %s\n", i, args[i]);
    setenv("__NV_PRIME_RENDER_OFFLOAD", "1", 1);
    setenv("__GLX_VENDOR_LIBRARY_NAME", "nvidia", 1);
    chdir(game_a[2].value);
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

static char *make_abs(const char *cwd, const char *path) {
  char *abs;
  m_asprintf(&abs, "%s/%s", cwd, path);
  return abs;
}

static void cp_append(char **cp, const char *cwd, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  char *rel;
  vasprintf(&rel, fmt, args);
  va_end(args);
  if (strstr(*cp, rel)) { free(rel); return; }
  char *old = *cp;
  if (**cp)
    m_asprintf(cp, "%s:%s/%s", old, cwd, rel);
  else
    m_asprintf(cp, "%s/%s", cwd, rel);
  free(old);
  free(rel);
}

static int analyze(void) {//TODO 重构为反箭头格式为卫语句
  if (access("instance.toml", F_OK) == 0) {
    if (access(".minecraft/versions/version.json", F_OK) == 0) {
      if (access(".moco/account.toml", F_OK) == 0) {
        printf("Login...\n");
        oauth2Refresh();
        char *cwd = getcwd(NULL, 0);
        jvm_a[0].value = make_abs(cwd, ".moco/natives");
        jvm_a[1].value = m_strdup("moco");
        jvm_a[2].value = m_strdup(MOCO_VERSION);
        jvm_a[3].value = m_strdup("");
        game_a[2].value = make_abs(cwd, ".minecraft");
        game_a[3].value = make_abs(cwd, ".minecraft/assets");
        game_a[7].value = m_strdup("");
        game_a[10].value = m_strdup("msa");
        profile_r = toml_parse_file_ex(".moco/profile.toml");
        toml_datum_t profile_root = profile_r.toptab;
        game_a[0].value = m_strdup(toml_seek(profile_root, "profile.name").u.s);
        version_json = file_to_json(".minecraft/versions/version.json");
        game_a[1].value = m_strdup(cJSON_GetObjectItemCaseSensitive(version_json, "id")->valuestring);
        game_a[4].value = m_strdup(cJSON_GetObjectItemCaseSensitive(version_json, "assets")->valuestring);
        mainClass = m_strdup(cJSON_GetObjectItemCaseSensitive(version_json, "mainClass")->valuestring);
        game_a[5].value = m_strdup(toml_seek(profile_root, "profile.id").u.s);
        account_r = toml_parse_file_ex(".moco/account.toml");
        toml_datum_t account_root = account_r.toptab;
        game_a[6].value = m_strdup(toml_seek(account_root, "account.minecraft_token").u.s);
        game_a[8].value = m_strdup(toml_seek(account_root, "account.uhs").u.s);
        game_a[9].value = m_strdup(cJSON_GetObjectItemCaseSensitive(version_json, "type")->valuestring);
        cJSON *version_arguments_json = cJSON_GetObjectItemCaseSensitive(version_json, "arguments");
        cJSON *version_arguments_game_json = cJSON_GetObjectItemCaseSensitive(version_arguments_json, "game");
        char *releaseTime = m_strdup(cJSON_GetObjectItemCaseSensitive(version_json, "releaseTime")->valuestring);

        cJSON *item;
        char flag;
        //default-user-jvm
        const int MC_26_1_SNAPSHOT_1_TIME_YEAR = 2025;
        //2025-12-16T12:42:29+00:00
        int releaseTime_YEAR;
        sscanf(releaseTime, "%d-", &releaseTime_YEAR);//TODO 自定义m_sscanf
        printf("%d", releaseTime_YEAR);
        if (releaseTime_YEAR > MC_26_1_SNAPSHOT_1_TIME_YEAR
          || strcmp("2025-12-16T12:42:29+00:00", releaseTime) == 0
          ) {
          cJSON *default_user_jvm_json = cJSON_GetObjectItemCaseSensitive(version_arguments_json, "default-user-jvm");
          cJSON *default_user_jvm_value_res = cJSON_GetArrayItem(default_user_jvm_json, 0);
          cJSON *default_user_jvm_value_json = cJSON_GetObjectItemCaseSensitive(default_user_jvm_value_res, "value");
          cJSON_ArrayForEach(item,default_user_jvm_value_json) {
            if (item->type == cJSON_String&&
              strstr(item->valuestring,"Xms")==NULL&&
              strstr(item->valuestring,"Xmx")==NULL)
              add_arg(&duj, item->valuestring);
          }
        }
        free(releaseTime);
        // game
        cJSON_ArrayForEach(item, version_arguments_game_json) {
          if (item->type == cJSON_String) {
            flag = 1;
            for (int i = 0; i < sizeof(game_a) / sizeof(m_string); ++i) {
              if (strcmp(item->valuestring, game_a[i].key) == 0) {
                add_arg(&game, game_a[i].value);
                flag = 0;
                break;
              }
            }
            if (flag) add_arg(&game, item->valuestring);
          }
        }
        cJSON *version_arguments_jvm_json = cJSON_GetObjectItemCaseSensitive(version_arguments_json, "jvm");
        cJSON *libraries_res = cJSON_GetObjectItemCaseSensitive(version_json, "libraries");
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
            cJSON *downloads_res = cJSON_GetObjectItemCaseSensitive(item, "downloads");
            cJSON *artifact_res = cJSON_GetObjectItemCaseSensitive(downloads_res, "artifact");
            cJSON *path_res = cJSON_GetObjectItemCaseSensitive(artifact_res, "path");
            cp_append(&jvm_a[3].value, cwd, ".minecraft/libraries/%s", path_res->valuestring);
          }
        }
        // java_path
        instance_r = toml_parse_file_ex("instance.toml");
        toml_datum_t instance_root = instance_r.toptab;
        toml_datum_t java_path_res = toml_seek(instance_root, "launch.java_path");
        if (java_path_res.type != TOML_UNKNOWN) {
          java_path = realpath(java_path_res.u.s, NULL);
          if (!java_path) java_path = m_strdup(java_path_res.u.s);
        } else if (access(".moco/java/bin/java", F_OK) == 0) {
          char *abs_java = realpath(".moco/java/bin/java", NULL);
          if (!abs_java) {
            perror("realpath"); exit(1);
          }
          java_path = abs_java;
        } else {
          free(cwd);
          return 1;
        }


        toml_datum_t forge = toml_seek(instance_root, "dependencies.forge");
        toml_datum_t neoforge = toml_seek(instance_root, "dependencies.neoforge");
        toml_datum_t fabric_loader = toml_seek(instance_root, "dependencies.fabric-loader");
        toml_datum_t quilt_loader = toml_seek(instance_root, "dependencies.quilt-loader");
        //TODO 应写为只允许加载一个
        if (forge.type == TOML_STRING || neoforge.type == TOML_STRING) {
          char *name;
          if (forge.type == TOML_STRING) {
            printf("Forge modloader detected: %s\n", forge.u.s);
            m_asprintf(&name, "%s-forge-%s", game_a[1].value, forge.u.s);
          } else {
            printf("NeoForge modloader detected: %s\n", neoforge.u.s);
            m_asprintf(&name, "neoforge-%s", neoforge.u.s);
          }

          char *json_path;
          m_asprintf(&json_path, ".minecraft/versions/%s/%s.json", name, name);
          cJSON *modloader_json = file_to_json(json_path);
          free(json_path);

          cJSON *modloader_arguments = cJSON_GetObjectItemCaseSensitive(modloader_json, "arguments");
          cJSON *modloader_game_a = cJSON_GetObjectItemCaseSensitive(modloader_arguments, "game");
          cJSON *modloader_jvm_a_res = cJSON_GetObjectItemCaseSensitive(modloader_arguments, "jvm");

          cJSON_ArrayForEach(item, modloader_game_a) add_arg(&game, item->valuestring);

          m_string modloader_jvm_a[] = {
            {"${version_name}", NULL},
            {"${library_directory}", NULL},
            {"${classpath_separator}", NULL},
          };
          modloader_jvm_a[0].value = m_strdup(game_a[1].value);
          modloader_jvm_a[1].value = make_abs(cwd, ".minecraft/libraries");
          modloader_jvm_a[2].value = m_strdup(":");

          cJSON_ArrayForEach(item, modloader_jvm_a_res) {
            char *resolved = m_strdup(item->valuestring);
            for (int i = 0; i < sizeof(modloader_jvm_a) / sizeof(m_string); ++i) {
              char *next = m_replace(resolved, modloader_jvm_a[i]);
              if (next != NULL) {
                free(resolved);
                resolved = next;
              }
            }
            add_arg(&jvm, resolved);
            free(resolved);
          }

          free(mainClass);
          mainClass = m_strdup(cJSON_GetObjectItemCaseSensitive(modloader_json, "mainClass")->valuestring);

          cJSON *modloader_libraries = cJSON_GetObjectItemCaseSensitive(modloader_json, "libraries");
          cJSON_ArrayForEach(item, modloader_libraries) {
            cJSON *downloads_res = cJSON_GetObjectItemCaseSensitive(item, "downloads");
            cJSON *artifact_res = cJSON_GetObjectItemCaseSensitive(downloads_res, "artifact");
            cJSON *path_res = cJSON_GetObjectItemCaseSensitive(artifact_res, "path");
            cp_append(&jvm_a[3].value, cwd, ".minecraft/libraries/%s", path_res->valuestring);
          }

          free(modloader_jvm_a[0].value);
          free(modloader_jvm_a[1].value);
          free(modloader_jvm_a[2].value);
          cJSON_Delete(modloader_json);
          free(name);
        }
        if (fabric_loader.type == TOML_STRING) {
          printf("Fabric Loader modloader detected: %s\n", fabric_loader.u.s);
          char *fabric_loader_json_path;
          m_asprintf(&fabric_loader_json_path,".minecraft/versions/fabric-loader-%s-%s.json",fabric_loader.u.s,game_a[1].value);
          cJSON *fabric_loader_json = file_to_json(fabric_loader_json_path);
          free(fabric_loader_json_path);
          cJSON *fabric_libraries_res = cJSON_GetObjectItemCaseSensitive(fabric_loader_json, "libraries");
          cJSON_ArrayForEach(item,fabric_libraries_res) {
            char *name = reMaven(cJSON_GetObjectItemCaseSensitive(item, "name")->valuestring);
            cp_append(&jvm_a[3].value, cwd, ".minecraft/libraries/%s", name);
            free(name);
          }

          free(mainClass);
          mainClass = m_strdup(cJSON_GetObjectItemCaseSensitive(fabric_loader_json, "mainClass")->valuestring);

          cJSON *fabric_jvm_res = cJSON_GetObjectItemCaseSensitive(
            cJSON_GetObjectItemCaseSensitive(fabric_loader_json,"arguments"), "jvm");
          cJSON_ArrayForEach(item,fabric_jvm_res)
            add_arg(&jvm, item->valuestring);
          cJSON_Delete(fabric_loader_json);
        }
        if (quilt_loader.type == TOML_STRING) {
          printf("Quilt Loader modloader detected: %s\n", quilt_loader.u.s);
        }

        if (forge.type != TOML_STRING && neoforge.type != TOML_STRING) {
          cp_append(&jvm_a[3].value, cwd, ".minecraft/versions/version/version.jar");
        }
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
            add_arg(&jvm, resolved);
            free(resolved);
          }
        }
        m_asprintf(&jvm_a_Xms, "-Xms%s", toml_seek(instance_root,"launch.jvm_xms").u.s);
        m_asprintf(&jvm_a_Xmx, "-Xmx%s", toml_seek(instance_root,"launch.jvm_xmx").u.s);
        add_arg(&jvm, jvm_a_Xms);
        add_arg(&jvm, jvm_a_Xmx);

        free(cwd);
      } else {
        fprintf(stderr, "Error: .moco/account.toml not found.\nPlease run 'moco login' first.\n");
        return 1;
      }
    } else {
      fprintf(stderr, "Error: version.json not found.\nPlease run 'moco install' first.\n");
      return 1;
    }
  } else {
    fprintf(stderr, "Error opening instance.toml: %s\n", result.errmsg);
    return 1;
  }
  return 0;
}