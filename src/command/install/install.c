//
// Created by root on 4/4/26.
//

#include "install.h"

#include "cJSON.h"
#include "command/search/search.h"
#include "m_exit.h"
#include "tomlc17.h"
#include "utility/download/m_epoll.h"
#include "utility/file/json.h"
#include "utility/file/toml.h"
#include "utility/minecraft/version.h"
#include "utility/mtool.h"
#include "utility/store/store.h"

#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

static toml_result_t result;
static toml_datum_t root;
static toml_datum_t version;
static int total = 0;
// const char *manifest_source;
// const char *assets_source;

// static void switchSource();
static int analyze(void);
static void installVersion(void);
static void download_asset(const Package *assetIndex);
static void download_libraries(const cJSON *version_json);
static int download_java();
static void installMods(void);
static int check_modloader(void);
static void installForge(const char *id);
static void installNeoforge(const char *id);
static void installFabric(const char *id);

void install(int argc, char *argv[]) {
  if (analyze() != 0) {
    m_exit(EX_DATAERR);
  }
  if (argc == 1) {
    installVersion_n();
    installDependencies();
    installMods();
  }
  if (argc != 1) {
    int opt;
    int option_index = 0;
    optind = 1;
    while ((opt = getopt_long(argc, argv, "+h", install_options, &option_index)) != -1) {
      switch (opt) {
      case 'h':
        printf("Usage: moco install [options]\n");
        printf("Options:\n");
        printf("  -h, --help    Show this help message\n");
        break;
      default:
        fprintf(stderr, "Unknown option `%c'\n", opt);
      }
    }
    char *subcommand = argv[optind];
    if (subcommand != NULL) {
      if (strcmp(subcommand, "forge") == 0) {
        installForge(argv[optind + 1]);
      }
      if (strcmp(subcommand, "neoforge") == 0) {
        installNeoforge(argv[optind + 1]);
      }
      if (strcmp(subcommand, "fabric") == 0) {
        installFabric(argv[optind + 1]);
      }
    }
  }
  toml_free(result);
}
// 引入LOCK文件锁来追踪安装状态
static int check_modloader(void) {
  toml_result_t r = toml_parse_file_ex("instance.toml");
  if (!r.ok)
    return 0;
  toml_datum_t dep = toml_seek(r.toptab, "dependencies.loader");
  if (dep.type == TOML_STRING) {
    toml_datum_t ver = toml_seek(r.toptab, "dependencies.loader_version");
    fprintf(stderr, "%s is already installed (%s)\n", dep.u.s,
            ver.type == TOML_STRING ? ver.u.s : "unknown");
    toml_free(r);
    return 1;
  }
  toml_free(r);
  return 0;
}

static void install_modloader(const char *key, const char *name, int is_fabric,
                              const char *id) {
  if (check_modloader() != 0)
    return;
  SearchResult *result = NULL;
  FILE *fp = fopen("instance.toml", "r+");
  if (id != NULL) {
    if (is_fabric)
      result = search_fabric(id);
    else if (strcmp(key, "forge") == 0)
      result = search_forge(id, L);
    else
      result = search_neoforge(id, L);
    if (result == NULL) {
      fprintf(stderr, "%s version %s not found\n", name, id);
      fclose(fp);
      m_exit(EX_DATAERR);
    }
    if (!is_fabric && strcmp(cJSON_GetObjectItemCaseSensitive(
                                 result->node[0], "version")
                                 ->valuestring,
                             version.u.s) != 0) {
      fprintf(stderr, "%s version %s is not compatible with Minecraft %s\n",
              name, cJSON_GetObjectItemCaseSensitive(result->node[0], "loader_version")->valuestring, version.u.s);
      fclose(fp);
      m_exit(EX_DATAERR);
    }
    if (is_fabric)
      printf("Warning: you are using a non-latest Fabric Loader version, "
             "unexpected bugs may occur\n");
    printf("%s version %s found\n", name,
           cJSON_GetObjectItemCaseSensitive(result->node[0],
                                            "loader_version")
               ->valuestring);
  } else {
    if (is_fabric)
      result = search_fabric("");
    else if (strcmp(key, "forge") == 0)
      result = search_forge(version.u.s, V);
    else
      result = search_neoforge(version.u.s, V);
    if (result == NULL) {
      fprintf(stderr, "%s version for Minecraft %s not found\n",
              name, version.u.s);
      fclose(fp);
      m_exit(EX_DATAERR);
    }
    printf("Latest %s version %s found\n", name,
           cJSON_GetObjectItemCaseSensitive(result->node[0],
                                            "loader_version")
               ->valuestring);
  }
  char *lv;
  m_asprintf(&lv, "loader = \"%s\"\nloader_version = \"%s\"",
             key, cJSON_GetObjectItemCaseSensitive(result->node[0], "loader_version")->valuestring);
  toml_add_on_table(fp, "[dependencies]", lv);
  free(lv);
  installVersion();
  fclose(fp);
  installDependencies();
  free_SearchResult(result);
}

void installForge(const char *id) { install_modloader("forge", "Forge", 0, id); }
void installNeoforge(const char *id) { install_modloader("neoforge", "NeoForge", 0, id); }
void installFabric(const char *id) { install_modloader("fabric-loader", "Fabric Loader", 1, id); }

static int analyze(void) {
  result = toml_parse_file_ex("instance.toml");
  if (!result.ok) {
    fprintf(stderr, "Error opening instance.toml: %s\n", result.errmsg);
    return -1;
  }
  root = result.toptab;
  version = toml_seek(root, "game.version");
  if (version.type != TOML_STRING) {
    fprintf(stderr, "The game version must be enclosed in quotation marks.\n");
    return -1;
  }
  if (has_version(version.u.s) != 0) {
    fprintf(stderr, "Minecraft version %s not found\n", version.u.s);
    return -1;
  }
  return 0;
}
/**
 *
 * @return 0 on success, 1 on failure
 */
int installVersion_n(void) {
  int rc = 0;
  toml_result_t instance = {0};
  SearchResult *result = NULL;
  Package *version_pkg = NULL;
  cJSON *version_json = NULL;
  FILE *lock_f = NULL;
  toml_result_t lock = {0};

  if (access("instance.toml", F_OK) != 0) {
    rc = 1;
    goto cleanup;
  }
  lock = toml_parse_file_ex("instance-lock.toml");
  toml_datum_t version_l = toml_seek(lock.toptab, "version");
  if (version_l.type == TOML_STRING) {
    printf("Version %s is already installed.\n", version_l.u.s);
    goto cleanup;
  }
  instance = toml_parse_file_ex("instance.toml");
  if (!instance.ok) {
    fprintf(stderr, "Error opening instance.toml: %s\n", instance.errmsg);
    rc = 1;
    goto cleanup;
  }
  toml_datum_t instance_r = instance.toptab;
  toml_datum_t version_n = toml_seek(instance_r, "game.version");
  if (version_n.type != TOML_STRING) {
    fprintf(stderr, "The game version must be enclosed in quotation marks.\n");
    rc = 1;
    goto cleanup;
  }
  if (has_version(version_n.u.s) != 0) {
    fprintf(stderr, "Minecraft version %s not found\n", version_n.u.s);
    rc = 1;
    goto cleanup;
  }
  result = search_version(version_n.u.s);
  if (result == NULL || result->count != 1) {
    fprintf(stderr, "Error: Version %s non-exact match\n", version_n.u.s);
    rc = 1;
    goto cleanup;
  }

  version_pkg = m_malloc(sizeof(Package));
  version_pkg->url = m_strdup(cJSON_GetObjectItemCaseSensitive(result->node[0], "url")->valuestring);
  version_pkg->sha1 = m_strdup(cJSON_GetObjectItemCaseSensitive(result->node[0], "sha1")->valuestring);
  version_pkg->path = m_strdup(".minecraft/versions/version.json");
  version_pkg->store = get_store_path(version_pkg->sha1);
  free_SearchResult(result);
  result = NULL;
  submit_download_task(version_pkg);
  wait_download_task(version_pkg->store);

  version_json = file_to_json(version_pkg->path);
  //libraries
  cJSON *libraries_res = cJSON_GetObjectItemCaseSensitive(version_json, "libraries");
  Package *library = m_malloc(sizeof(Package));
  cJSON *lib_item = NULL;
  cJSON_ArrayForEach(lib_item, libraries_res) {
    int flag = 1;
    cJSON *rules_res = cJSON_GetObjectItemCaseSensitive(lib_item, "rules");
    cJSON *rule_res = cJSON_GetArrayItem(rules_res, 0);
    cJSON *os_res = cJSON_GetObjectItemCaseSensitive(rule_res, "os");
    cJSON *name_res = cJSON_GetObjectItemCaseSensitive(os_res, "name");
    if (name_res)
      if (strcmp(name_res->valuestring, "linux") != 0)
        flag = 0;
    if (flag) {
      cJSON *download_res = cJSON_GetObjectItemCaseSensitive(lib_item, "downloads");
      cJSON *artifact_res = cJSON_GetObjectItemCaseSensitive(download_res, "artifact");
      cJSON *path_res = cJSON_GetObjectItemCaseSensitive(artifact_res, "path");
      cJSON *sha1_res = cJSON_GetObjectItemCaseSensitive(artifact_res, "sha1");
      cJSON *url_res = cJSON_GetObjectItemCaseSensitive(artifact_res, "url");
      char *lib_path, *lib_store;
      m_asprintf(&lib_path, ".minecraft/libraries/%s", path_res->valuestring);
      lib_store = get_store_path(sha1_res->valuestring);
      library->store = lib_store;
      library->path = lib_path;
      library->sha1 = m_strdup(sha1_res->valuestring);
      library->url = m_strdup(url_res->valuestring);
      submit_download_task(library);
      free(library->store);
      free(library->path);
      free(library->sha1);
      free(library->url);
    }
  }
  free(library);
  //logging
  cJSON *logging_res = cJSON_GetObjectItemCaseSensitive(version_json, "logging");
  cJSON *logging_client_res = cJSON_GetObjectItemCaseSensitive(logging_res, "client");
  cJSON *logging_file_rs = cJSON_GetObjectItemCaseSensitive(logging_client_res, "file");
  cJSON *logging_id = cJSON_GetObjectItemCaseSensitive(logging_file_rs, "id");
  cJSON *logging_sha1 = cJSON_GetObjectItemCaseSensitive(logging_file_rs, "sha1");
  cJSON *logging_url = cJSON_GetObjectItemCaseSensitive(logging_file_rs, "url");
  Package *logging_pkg = m_malloc(sizeof(Package));
  char *log_path;
  m_asprintf(&log_path, ".minecraft/assets/log_configs/%s", logging_id->valuestring);
  logging_pkg->store = get_store_path(logging_sha1->valuestring);
  logging_pkg->path = log_path;
  logging_pkg->sha1 = m_strdup(logging_sha1->valuestring);
  logging_pkg->url = m_strdup(logging_url->valuestring);
  submit_download_task(logging_pkg);
  free_package(logging_pkg);
  //version.jar
  cJSON *downloads_res = cJSON_GetObjectItemCaseSensitive(version_json, "downloads");
  cJSON *client_res = cJSON_GetObjectItemCaseSensitive(downloads_res, "client");
  cJSON *client_sha1_res = cJSON_GetObjectItemCaseSensitive(client_res, "sha1");
  cJSON *client_url_res = cJSON_GetObjectItemCaseSensitive(client_res, "url");
  Package *client_pkg = m_malloc(sizeof(Package));
  client_pkg->path = m_strdup(".minecraft/versions/version/version.jar");
  client_pkg->sha1 = m_strdup(client_sha1_res->valuestring);
  client_pkg->url = m_strdup(client_url_res->valuestring);
  client_pkg->store = get_store_path(client_pkg->sha1);
  submit_download_task(client_pkg);
  free_package(client_pkg);
  //asset
  cJSON *assetIndex_res = cJSON_GetObjectItemCaseSensitive(version_json, "assetIndex");
  cJSON *assetIndex_id_res = cJSON_GetObjectItemCaseSensitive(assetIndex_res, "id");
  cJSON *assetIndex_sha1_res = cJSON_GetObjectItemCaseSensitive(assetIndex_res, "sha1");
  cJSON *assetIndex_url_res = cJSON_GetObjectItemCaseSensitive(assetIndex_res, "url");
  Package *assetIndex_pkg = m_malloc(sizeof(Package));
  char *assetIndex_path;
  m_asprintf(&assetIndex_path, ".minecraft/assets/indexes/%s.json", assetIndex_id_res->valuestring);
  assetIndex_pkg->path = m_strdup(assetIndex_path);
  free(assetIndex_path);
  assetIndex_pkg->url = m_strdup(assetIndex_url_res->valuestring);
  assetIndex_pkg->sha1 = m_strdup(assetIndex_sha1_res->valuestring);
  assetIndex_pkg->store = get_store_path(assetIndex_pkg->sha1);
  submit_download_task(assetIndex_pkg);
  wait_download_task(assetIndex_pkg->store);

  cJSON *asset_json = file_to_json(assetIndex_pkg->path);
  if (!asset_json) {
    fprintf(stderr, "Error analyze assetIndex.json\n");
    rc = 1;
    goto cleanup;
  }
  cJSON *objects_res = cJSON_GetObjectItem(asset_json, "objects");
  cJSON *asset_item = NULL;
  Package *asset_pack = m_malloc(sizeof(Package));
  cJSON_ArrayForEach(asset_item, objects_res) {
    cJSON *hash_res = cJSON_GetObjectItemCaseSensitive(asset_item, "hash");
    asset_pack->sha1 = m_strdup(hash_res->valuestring);
    char *asset_url;
    m_asprintf(&asset_url, "https://resources.download.minecraft.net/%c%c/%s",
               hash_res->valuestring[0],
               hash_res->valuestring[1],
               hash_res->valuestring);
    asset_pack->url = asset_url;
    char *asset_path;
    m_asprintf(&asset_path, ".minecraft/assets/objects/%c%c/%s",
               hash_res->valuestring[0],
               hash_res->valuestring[1],
               hash_res->valuestring);
    asset_pack->path = asset_path;
    asset_pack->store = get_store_path(asset_pack->sha1);
    submit_download_task(asset_pack);
    free(asset_pack->store);
    free(asset_pack->path);
    free(asset_pack->sha1);
    free(asset_pack->url);
  }
  free(asset_pack);
  cJSON_Delete(asset_json);

  free_package(assetIndex_pkg);

  wait_epoll_download_task();
  if (access("instance-lock.toml",F_OK) != 0) fclose(fopen("instance-lock.toml", "w"));
  lock_f = fopen("instance-lock.toml","rb+");
  toml_add_on_toptab(lock_f,"version",version_n.u.s);

cleanup:
  toml_free(lock);
  if (lock_f) fclose(lock_f);
  cJSON_Delete(version_json);
  free_package(version_pkg);
  free_SearchResult(result);
  toml_free(instance);
  return rc;
}

static void installVersion() {
  // Package *version_pkg = get_version(version.u.s);
  SearchResult *version_search_result = search_version(version.u.s);
  if (version_search_result == NULL) {
    fprintf(stderr, "Error: Version %s not found\n"
                    "Check your instance.toml or run moco update",
            version.u.s);
    m_exit(EX_DATAERR);
  }
  if (version_search_result->count != 1) {
    fprintf(stderr, "Error: Version %s non-exact match\n", version.u.s);
    m_exit(EX_DATAERR);
  }
  Package *version_pkg = m_malloc(sizeof(Package));
  version_pkg->url = m_strdup(cJSON_GetObjectItemCaseSensitive(version_search_result->node[0], "url")->valuestring);
  version_pkg->sha1 = m_strdup(cJSON_GetObjectItemCaseSensitive(version_search_result->node[0], "sha1")->valuestring);
  version_pkg->path = m_strdup(".minecraft/versions/version.json");
  version_pkg->store = get_store_path(version_pkg->sha1);
  free_SearchResult(version_search_result);
  submit_download_task(version_pkg);
  wait_download_task(version_pkg->store);

  cJSON *version_json = file_to_json(version_pkg->path);

  download_libraries(version_json);

  cJSON *logging_res = cJSON_GetObjectItemCaseSensitive(version_json, "logging");
  cJSON *logging_client_res = cJSON_GetObjectItemCaseSensitive(logging_res, "client");
  cJSON *logging_file_rs = cJSON_GetObjectItemCaseSensitive(logging_client_res, "file");
  cJSON *logging_id = cJSON_GetObjectItemCaseSensitive(logging_file_rs, "id");
  cJSON *logging_sha1 = cJSON_GetObjectItemCaseSensitive(logging_file_rs, "sha1");
  cJSON *logging_url = cJSON_GetObjectItemCaseSensitive(logging_file_rs, "url");
  Package *logging_pkg = m_malloc(sizeof(Package));
  char *path, *store;
  m_asprintf(&path, ".minecraft/assets/log_configs/%s", logging_id->valuestring);
  store = get_store_path(logging_sha1->valuestring);
  logging_pkg->store = store;
  logging_pkg->path = path;
  logging_pkg->sha1 = m_strdup(logging_sha1->valuestring);
  logging_pkg->url = m_strdup(logging_url->valuestring);
  submit_download_task(logging_pkg);
  free_package(logging_pkg);

  cJSON *downloads_res = cJSON_GetObjectItemCaseSensitive(version_json, "downloads");
  cJSON *client_res = cJSON_GetObjectItemCaseSensitive(downloads_res, "client");
  cJSON *client_sha1_res = cJSON_GetObjectItemCaseSensitive(client_res, "sha1");
  cJSON *client_url_res = cJSON_GetObjectItemCaseSensitive(client_res, "url");
  Package *client_pkg = m_malloc(sizeof(Package));
  client_pkg->path = m_strdup(".minecraft/versions/version/version.jar");
  client_pkg->sha1 = m_strdup(client_sha1_res->valuestring);
  client_pkg->url = m_strdup(client_url_res->valuestring);
  client_pkg->store = get_store_path(client_pkg->sha1);
  submit_download_task(client_pkg);
  free_package(client_pkg);

  cJSON *assetIndex_res = cJSON_GetObjectItemCaseSensitive(version_json, "assetIndex");
  cJSON *assetIndex_id_res = cJSON_GetObjectItemCaseSensitive(assetIndex_res, "id");
  cJSON *assetIndex_sha1_res = cJSON_GetObjectItemCaseSensitive(assetIndex_res, "sha1");
  cJSON *assetIndex_url_res = cJSON_GetObjectItemCaseSensitive(assetIndex_res, "url");
  Package *assetIndex_pkg = m_malloc(sizeof(Package));
  char *assetIndex_path;
  m_asprintf(&assetIndex_path, ".minecraft/assets/indexes/%s.json", assetIndex_id_res->valuestring);
  assetIndex_pkg->path = m_strdup(assetIndex_path);
  free(assetIndex_path);
  assetIndex_pkg->url = m_strdup(assetIndex_url_res->valuestring);
  assetIndex_pkg->sha1 = m_strdup(assetIndex_sha1_res->valuestring);
  assetIndex_pkg->store = get_store_path(assetIndex_pkg->sha1);
  submit_download_task(assetIndex_pkg);

  wait_download_task(assetIndex_pkg->store);

  download_asset(assetIndex_pkg);
  free_package(assetIndex_pkg);
  cJSON_Delete(version_json);
  free_package(version_pkg);

  wait_epoll_download_task();
}

static void download_asset(const Package *assetIndex) {
  cJSON *asset_json = file_to_json(assetIndex->path);
  if (asset_json) {
    cJSON *objects_res = cJSON_GetObjectItem(asset_json, "objects");
    cJSON *item = NULL;
    Package *pack = malloc(sizeof(Package));
    cJSON_ArrayForEach(item, objects_res) {
      cJSON *hash_res = cJSON_GetObjectItemCaseSensitive(item, "hash");
      char *url;
      pack->sha1 = m_strdup(hash_res->valuestring);
      m_asprintf(&url, "https://resources.download.minecraft.net/%c%c/%s",
                 hash_res->valuestring[0],
                 hash_res->valuestring[1],
                 hash_res->valuestring);
      pack->url = url;
      char *path;
      m_asprintf(&path, ".minecraft/assets/objects/%c%c/%s",
                 hash_res->valuestring[0],
                 hash_res->valuestring[1],
                 hash_res->valuestring);
      pack->path = path;
      pack->store = get_store_path(pack->sha1);
      submit_download_task(pack);
      free(pack->store);
      free(pack->path);
      free(pack->sha1);
      free(pack->url);
    }
    free(pack);
    cJSON_Delete(asset_json);
  } else {
    printf("Error analyze assetIndex.json\n");
    m_exit(EX_DATAERR); // TODO 使用sysexits重写所有函数
  }
}

static void download_libraries(const cJSON *version_json) {
  const cJSON *libraries_res = cJSON_GetObjectItemCaseSensitive(version_json, "libraries");
  Package *library = m_malloc(sizeof(Package));
  cJSON *item = NULL;
  cJSON_ArrayForEach(item, libraries_res) {
    int flag = 1;
    cJSON *rules_res = cJSON_GetObjectItemCaseSensitive(item, "rules");
    cJSON *rule_res = cJSON_GetArrayItem(rules_res, 0);
    cJSON *os_res = cJSON_GetObjectItemCaseSensitive(rule_res, "os");
    cJSON *name_res = cJSON_GetObjectItemCaseSensitive(os_res, "name");
    if (name_res)
      if (strcmp(name_res->valuestring, "linux") != 0)
        flag = 0;
    if (flag) {
      cJSON *download_res = cJSON_GetObjectItemCaseSensitive(item, "downloads");
      cJSON *artifact_res = cJSON_GetObjectItemCaseSensitive(download_res, "artifact");
      cJSON *path_res = cJSON_GetObjectItemCaseSensitive(artifact_res, "path");
      cJSON *sha1_res = cJSON_GetObjectItemCaseSensitive(artifact_res, "sha1");
      cJSON *url_res = cJSON_GetObjectItemCaseSensitive(artifact_res, "url");
      char *path, *store;
      m_asprintf(&path, ".minecraft/libraries/%s", path_res->valuestring);
      store = get_store_path(sha1_res->valuestring);
      library->store = store;
      library->path = path;
      library->sha1 = m_strdup(sha1_res->valuestring);
      library->url = m_strdup(url_res->valuestring);
      submit_download_task(library);
      free(library->store);
      free(library->path);
      free(library->sha1);
      free(library->url);
    }
  }
  free(library);
}

int download_java() {
  cJSON *version_json = file_to_json(".minecraft/versions/version.json");
  if (version_json) {
    cJSON *javaVersion_res = cJSON_GetObjectItemCaseSensitive(version_json, "javaVersion");
    cJSON *majorVersion_res = cJSON_GetObjectItemCaseSensitive(javaVersion_res, "majorVersion");
    char *url;
    if (majorVersion_res->valueint == 16)
      m_asprintf(&url, "https://api.adoptium.net/v3/binary/latest/17/ga/linux/x64/jre/hotspot/normal/eclipse");
    else
      m_asprintf(&url,
                 "https://api.adoptium.net/v3/binary/latest/%d/ga/linux/x64/jre/hotspot/normal/eclipse",
                 majorVersion_res->valueint);
    Package *java = m_malloc(sizeof(Package));
    java->url = url;
    java->store = m_strdup(".moco/java.tar.gz");
    java->path = m_strdup(".moco/java.tar.gz");
    java->sha1 = m_strdup("-1");
    submit_download_task(java);
    wait_epoll_download_task();

    if (access(".moco/java.tar.gz", F_OK) == 0) {
      mkdirs(".moco/java", R);
      system("tar -xzf .moco/java.tar.gz -C .moco/java --strip-components=1");
    }
    cJSON_Delete(version_json);
    free_package(java);
  } else {
    fprintf(stderr, "please run command install first\n");
    return 1;
  }
  return 0;
}

int installDependencies(void) {
  int rc = 0;
  toml_result_t r = {0};
  FILE *lock_f = NULL;
  toml_result_t lock = {0};

  r = toml_parse_file_ex("instance.toml");
  if (!r.ok) {
    fprintf(stderr, "Error opening instance.toml: %s\n", r.errmsg);
    rc = 1;
    goto cleanup;
  }
  lock = toml_parse_file_ex("instance-lock.toml");
  toml_datum_t loader_l = toml_get(lock.toptab, "loader");
  if (loader_l.type == TOML_STRING) {
    fprintf(stderr, "%s is already installed.\n", loader_l.u.s);
    goto cleanup;
  }
  toml_datum_t rt = r.toptab;
  toml_datum_t ver = toml_seek(rt, "game.version");
  if (ver.type != TOML_STRING) {
    fprintf(stderr, "The game version must be enclosed in quotation marks.\n");
    rc = 1;
    goto cleanup;
  }
  toml_datum_t loader_type = toml_seek(rt, "dependencies.loader");
  toml_datum_t loader_ver = toml_seek(rt, "dependencies.loader_version");
  if (loader_type.type != TOML_STRING) {
    rc = 1;
    goto cleanup;
  }
  lock_f = fopen("instance-lock.toml","rb+");
  if (strcmp(loader_type.u.s, "forge") == 0) {
    if (download_java() != 0) {
      fprintf(stderr, "Error downloading Java runtime, cannot continue installing Forge\n");
      rc = 1;
      goto cleanup;
    }
    Package *forge_installer_package = m_malloc(sizeof(Package));
    forge_installer_package->path = m_strdup(".moco/forge-installer.jar");
    forge_installer_package->sha1 = m_strdup("-1");
    forge_installer_package->store = m_strdup(".moco/forge-installer.jar");
    char *url;
    m_asprintf(&url, "https://maven.minecraftforge.net/net/minecraftforge/forge/%s-%s/forge-%s-%s-installer.jar",
               ver.u.s, loader_ver.u.s, ver.u.s, loader_ver.u.s);
    forge_installer_package->url = url;
    submit_download_task(forge_installer_package);
    wait_epoll_download_task();
    free_package(forge_installer_package);

    FILE *file = fopen(".minecraft/launcher_profiles.json", "wb+");
    fprintf(file, "{\n"
                  "\t\"profiles\":{}\n"
                  "}");
    fclose(file);
    int status = system(".moco/java/bin/java -jar .moco/forge-installer.jar --installClient .minecraft");
    if (WEXITSTATUS(status) != 0) {
      fprintf(stderr, "Error installing Forge, installer exited with code %d\n", status);
      rc = 1;
      goto cleanup;
    }
    toml_add_on_toptab(lock_f,"loader","forge");
    toml_add_on_toptab(lock_f,"loader_version",loader_ver.u.s);
  }
  if (strcmp(loader_type.u.s, "neoforge") == 0) {
    if (download_java() != 0) {
      fprintf(stderr, "Error downloading Java runtime, cannot continue installing NeoForge\n");
      rc = 1;
      goto cleanup;
    }
    Package *neoforge_installer_package = m_malloc(sizeof(Package));
    neoforge_installer_package->path = m_strdup(".moco/neoforge-installer.jar");
    neoforge_installer_package->sha1 = m_strdup("-1");
    neoforge_installer_package->store = m_strdup(".moco/neoforge-installer.jar");
    char *url;
    m_asprintf(&url, "https://maven.neoforged.net/releases/net/neoforged/neoforge/%s/neoforge-%s-installer.jar", loader_ver.u.s, loader_ver.u.s);
    neoforge_installer_package->url = url;
    submit_download_task(neoforge_installer_package);
    wait_epoll_download_task();
    free_package(neoforge_installer_package);
    FILE *file = fopen(".minecraft/launcher_profiles.json", "wb+");
    fprintf(file, "{\n"
                  "\t\"profiles\":{}\n"
                  "}");
    fclose(file);
    int status = system(".moco/java/bin/java -jar .moco/neoforge-installer.jar --installClient .minecraft");
    if (WEXITSTATUS(status) != 0) {
      fprintf(stderr, "Error installing NeoForge, installer exited with code %d\n", status);
      rc = 1;
      goto cleanup;
    }
    toml_add_on_toptab(lock_f, "loader", "neoforge");
    toml_add_on_toptab(lock_f, "loader_version", loader_ver.u.s);
  }
  if (strcmp(loader_type.u.s, "fabric-loader") == 0) {
    Package *fabric_loader_package = m_malloc(sizeof(Package));
    char *url;
    m_asprintf(&url, "https://meta.fabricmc.net/v2/versions/loader/%s/%s/profile/json", ver.u.s, loader_ver.u.s);
    char *path;
    m_asprintf(&path, ".minecraft/versions/fabric-loader-%s-%s.json", loader_ver.u.s, ver.u.s);
    fabric_loader_package->path = path;
    fabric_loader_package->sha1 = m_strdup("-1");
    fabric_loader_package->url = url;
    fabric_loader_package->store = m_strdup(path);
    submit_download_task(fabric_loader_package);
    wait_epoll_download_task();

    cJSON *json = file_to_json(path);
    free_package(fabric_loader_package);
    char *inheritsFrom = cJSON_GetObjectItemCaseSensitive(json, "inheritsFrom")->valuestring;
    if (strcmp(ver.u.s, inheritsFrom) != 0) {
      fprintf(stderr, "Error: Fabric Loader version %s does not match game version %s\n", loader_ver.u.s, ver.u.s);
      cJSON_Delete(json);
      rc = 1;
      goto cleanup;
    }
    cJSON *libraries = cJSON_GetObjectItemCaseSensitive(json, "libraries");
    cJSON *item;
    Package *libraries_package = m_malloc(sizeof(Package));
    cJSON_ArrayForEach(item, libraries) {
      char *name = reMaven(cJSON_GetObjectItemCaseSensitive(item, "name")->valuestring);
      char *path_l;
      m_asprintf(&path_l, ".minecraft/libraries/%s", name);
      char *url_l;
      m_asprintf(&url_l, "https://maven.fabricmc.net/%s", name);
      cJSON *sha1_l = cJSON_GetObjectItemCaseSensitive(item, "sha1");

      libraries_package->path = path_l;
      libraries_package->url = url_l;
      if (sha1_l != NULL) {
        libraries_package->sha1 = m_strdup(sha1_l->valuestring);
        libraries_package->store = get_store_path(sha1_l->valuestring);
      } else {
        libraries_package->sha1 = m_strdup("-1");
        libraries_package->store = m_strdup(path_l);
      }

      submit_download_task(libraries_package);
      free(name);
      free(libraries_package->path);
      free(libraries_package->sha1);
      free(libraries_package->url);
      free(libraries_package->store);
    }
    free(libraries_package);
    cJSON_Delete(json);
    toml_add_on_toptab(lock_f, "loader", "fabric-loader");
    toml_add_on_toptab(lock_f, "loader_version", loader_ver.u.s);
  }
  if (strcmp(loader_type.u.s, "quilt-loader") == 0) {
    printf("此加载器方式暂时被搁置\n");
  }

cleanup:
  toml_free(lock);
  if (lock_f) fclose(lock_f);
  toml_free(r);
  return rc;
}

void installMods() {
  toml_result_t r = toml_parse_file_ex("instance.toml");
  if (!r.ok)
    return;
  toml_datum_t rt = r.toptab;
  toml_datum_t mods = toml_seek(rt, "mods");
  if (mods.type != TOML_ARRAY) {
    toml_free(r);
    return;
  }
  Package *mod_package = m_malloc(sizeof(Package));
  for (int i = 0; i < mods.u.arr.size; i++) {
    toml_datum_t mod = mods.u.arr.elem[i];
    mod_package->path = m_strdup(toml_get(mod, "path").u.s);
    mod_package->sha1 = m_strdup(toml_get(mod, "sha1").u.s);
    mod_package->url = m_strdup(toml_get(mod, "url").u.s);
    mod_package->store = get_store_path(mod_package->sha1);
    submit_download_task(mod_package);
    free(mod_package->path);
    free(mod_package->sha1);
    free(mod_package->url);
    free(mod_package->store);
  }
  free(mod_package);
  toml_free(r);
}