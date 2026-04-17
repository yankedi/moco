//
// Created by root on 4/4/26.
//

#include "install.h"

#include "cjson/cJSON.h"
#include "command/search/search.h"
#include "m_exit.h"
#include "tomlc17.h"
#include "utility/download/m_epoll.h"
#include "utility/file/json.h"
#include "utility/mtool.h"
#include "utility/store/store.h"

#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

toml_result_t result;
toml_datum_t root;
toml_datum_t version;
int total = 0;
// const char *manifest_source;
// const char *assets_source;

// static void switchSource();
static int analyze(void);
static void installVersion(void);
static void download_asset(const package *assetIndex);
static void download_libraries(const cJSON *version_json);

void install(void) {
  if (analyze()) {
    installVersion();
  }
  toml_free(result);
}
static int analyze(void) {
  result = toml_parse_file_ex("instance.toml");
  if (!result.ok) {
    fprintf(stderr, "Error opening instance.toml: %s\n", result.errmsg);
    return 0;
  }
  root = result.toptab;
  version = toml_seek(root, "game.version");
  if (version.type != TOML_STRING) {
    fprintf(stderr, "The game version must be enclosed in quotation marks.\n");
    return 0;
  }
  return 1;
}

static void installVersion() {
  printf("[Install] 获取版本元数据: %s\n", version.u.s);
  package *version_pkg = get_version(version.u.s);
  submit_download_task(version_pkg);
  wait_download_task(version_pkg->store);

  cJSON *version_json = file_to_json(version_pkg->path);

  printf("[Install] 提交基础组件下载任务（libraries / logging / client）\n");
  download_libraries(version_json);

  printf("[Install] 提交 logging 配置下载任务\n");
  cJSON *logging_res = cJSON_GetObjectItemCaseSensitive(version_json, "logging");
  cJSON *logging_client_res = cJSON_GetObjectItemCaseSensitive(logging_res, "client");
  cJSON *logging_file_rs = cJSON_GetObjectItemCaseSensitive(logging_client_res, "file");
  cJSON *logging_id = cJSON_GetObjectItemCaseSensitive(logging_file_rs, "id");
  cJSON *logging_sha1 = cJSON_GetObjectItemCaseSensitive(logging_file_rs, "sha1");
  cJSON *logging_url = cJSON_GetObjectItemCaseSensitive(logging_file_rs, "url");
  package *logging_pkg = m_malloc(sizeof(package));
  char *path, *store;
  m_asprintf(&path, ".minecraft/assets/log_configs/%s",logging_id->valuestring);
  store = get_store_path(logging_sha1->valuestring);
  logging_pkg->store = store;
  logging_pkg->path = path;
  logging_pkg->sha1 = m_strdup(logging_sha1->valuestring);
  logging_pkg->url = m_strdup(logging_url->valuestring);
  submit_download_task(logging_pkg);
  free_package(logging_pkg);

  printf("[Install] 提交 client 下载任务\n");
  cJSON *downloads_res = cJSON_GetObjectItemCaseSensitive(version_json, "downloads");
  cJSON *client_res = cJSON_GetObjectItemCaseSensitive(downloads_res, "client");
  cJSON *client_sha1_res = cJSON_GetObjectItemCaseSensitive(client_res, "sha1");
  cJSON *client_url_res = cJSON_GetObjectItemCaseSensitive(client_res, "url");
  package *client_pkg = m_malloc(sizeof(package));
  client_pkg->path = m_strdup(".minecraft/versions/version/version.jar");
  client_pkg->sha1 = m_strdup(client_sha1_res->valuestring);
  client_pkg->url = m_strdup(client_url_res->valuestring);
  client_pkg->store = get_store_path(client_pkg->sha1);
  submit_download_task(client_pkg);
  free_package(client_pkg);

  printf("[Install] 提交资源索引下载任务\n");
  cJSON *assetIndex_res = cJSON_GetObjectItemCaseSensitive(version_json, "assetIndex");
  cJSON *assetIndex_id_res = cJSON_GetObjectItemCaseSensitive(assetIndex_res, "id");
  cJSON *assetIndex_sha1_res = cJSON_GetObjectItemCaseSensitive(assetIndex_res, "sha1");
  cJSON *assetIndex_url_res = cJSON_GetObjectItemCaseSensitive(assetIndex_res, "url");
  package *assetIndex_pkg = m_malloc(sizeof(package));
  char *assetIndex_path;
  m_asprintf(&assetIndex_path, ".minecraft/assets/indexes/%s.json",assetIndex_id_res->valuestring);
  assetIndex_pkg->path = m_strdup(assetIndex_path);
  free(assetIndex_path);
  assetIndex_pkg->url = m_strdup(assetIndex_url_res->valuestring);
  assetIndex_pkg->sha1 = m_strdup(assetIndex_sha1_res->valuestring);
  assetIndex_pkg->store = get_store_path(assetIndex_pkg->sha1);
  submit_download_task(assetIndex_pkg);

  wait_download_task(assetIndex_pkg->store);

  printf("[Install] 提交资源对象下载任务\n");
  download_asset(assetIndex_pkg);
  free_package(assetIndex_pkg);
  cJSON_Delete(version_json);
  free_package(version_pkg);

  printf("[Install] 基础组件已在后台下载，资源对象已开始提交\n");
}

static void download_asset(const package *assetIndex) {
  cJSON *asset_json = file_to_json(assetIndex->path);
  if (asset_json) {
    cJSON *objects_res = cJSON_GetObjectItem(asset_json, "objects");
    cJSON *item = NULL;
    package *pack = malloc(sizeof(package));
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
  package *library = m_malloc(sizeof(package));
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
    package *java = m_malloc(sizeof(package));
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