//
// Created by root on 4/5/26.
//

#include "search.h"
#include "_deps/cjson-src/cJSON.h"
#include "env.h"
#include "interface.h"
#include "utility/file/json.h"
#include "utility/mtool.h"
#include "utility/store/store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void search(int argc, char *argv[]) {
  int opt;
  int option_index = 0;
  optind = 1;
  while ((opt = getopt_long(argc, argv, "+h", search_options, &option_index)) !=
         -1) {
    switch (opt) {
    case 'h':
      printf("Usage: moco search [command]/[options]\n");
      printf("Options:\n");
      printf("  -h, --help    Show this help message\n");
      break;
    default:
      fprintf(stderr, "Unknown option: %c\n", opt);
    }
  }
  char *subcommand = argv[optind];
  if (subcommand != NULL) {
    if (strcmp(subcommand, "version") == 0) {
      SearchResult *result = search_versions(argv[optind + 1]);
      free_SearchResult(result);
      // package *version_tmp = get_version(argv[optind + 1]);
      // free_package(version_tmp);
    }
  }
}

SearchResult *search_versions(const char *v) {
  char *path = NULL;
  m_asprintf(&path, "%s/version_manifest_v2.json", XDG_DATA_HOME);
  cJSON *manifest = file_to_json(path);
  free(path);
  if (manifest == NULL) {
    fprintf(stderr, "Please use the \"moco update\"command first.\n");
    return NULL;
  }
  const cJSON *versions = cJSON_GetObjectItem(manifest, "versions");
  const int versions_size = cJSON_GetArraySize(versions);
  SearchResult *result = m_malloc(sizeof(SearchResult));
  result->node = NULL;
  result->count = 0;
  for (int i = 0; i < versions_size; ++i) {
    cJSON *version = cJSON_GetArrayItem(versions, i);
    cJSON *id = cJSON_GetObjectItem(version, "id");
    if (strstr(id->valuestring, v) != NULL) {
        result->node = realloc(result->node, sizeof(cJSON *) * (result->count + 1));//TODO 自定义m_realloc函数
        result->node[result->count] = cJSON_Duplicate(version, 1);
        result->count++;
    }
  }
  if (result->count != 0) {
    for (int i = 0;i < result->count;++i) {
      printf("version: %s\n", cJSON_GetObjectItemCaseSensitive(result->node[i], "id")->valuestring);
      printf("type: %s\n", cJSON_GetObjectItemCaseSensitive(result->node[i], "type")->valuestring);
      printf("release time: %s\n", cJSON_GetObjectItemCaseSensitive(result->node[i], "releaseTime")->valuestring);
      printf("\n");
    }
    cJSON_Delete(manifest);
    return result;
  }
  cJSON_Delete(manifest);
  return NULL;
}

SearchResult *search_version(const char *v) {
  char *path = NULL;
  m_asprintf(&path, "%s/version_manifest_v2.json", XDG_DATA_HOME);
  cJSON *manifest = file_to_json(path);
  free(path);
  if (manifest == NULL) {
    fprintf(stderr, "Please use the \"moco update\"command first.\n");
    return NULL;
  }
  const cJSON *versions = cJSON_GetObjectItem(manifest, "versions");
  const int versions_size = cJSON_GetArraySize(versions);
  SearchResult *result = m_malloc(sizeof(SearchResult));
  result->node = NULL;
  result->count = 0;
  for (int i = 0; i < versions_size; ++i) {
    cJSON *version = cJSON_GetArrayItem(versions, i);
    cJSON *id = cJSON_GetObjectItem(version, "id");
    if (strcmp(id->valuestring, v) == 0) {
      result->node = realloc(result->node, sizeof(cJSON *) * (result->count + 1));
      result->node[result->count] = cJSON_Duplicate(version, 1);
      result->count++;
      break;
    }
  }
  if (result->count != 0) {
    printf("version: %s\n", cJSON_GetObjectItemCaseSensitive(result->node[0], "id")->valuestring);
    printf("type: %s\n", cJSON_GetObjectItemCaseSensitive(result->node[0], "type")->valuestring);
    printf("release time: %s\n", cJSON_GetObjectItemCaseSensitive(result->node[0], "releaseTime")->valuestring);
    printf("\n");
    cJSON_Delete(manifest);
    return result;
  }
  cJSON_Delete(manifest);
  return NULL;
}

#if 0
package *get_version(const char *v) {
  package *p = m_malloc(sizeof(package));
  p->url = NULL;
  p->sha1 = NULL;
  p->path = NULL;
  char *path = NULL;
  m_asprintf(&path, "%s/version_manifest_v2.json", XDG_DATA_HOME);
  cJSON *manifest = file_to_json(path);
  free(path);

  if (manifest == NULL) {
    fprintf(stderr, "Please use the \"moco update\"command first.\n");
    cJSON_Delete(manifest);
    free(p);
    return NULL;
  }

  const cJSON *versions = cJSON_GetObjectItem(manifest, "versions");
  const int versions_size = cJSON_GetArraySize(versions);
  cJSON *target_version_node = NULL;
  for (int i = 0; i < versions_size; ++i) {
    cJSON *version = cJSON_GetArrayItem(versions, i);
    cJSON *id = cJSON_GetObjectItem(version, "id");
    if (strcmp(id->valuestring, v) == 0) {
      target_version_node = version;
      break;
    }
  }
  if (target_version_node != NULL) {
    cJSON *id = cJSON_GetObjectItem(target_version_node, "id");
    cJSON *type = cJSON_GetObjectItem(target_version_node, "type");
    cJSON *releaseTime =
        cJSON_GetObjectItem(target_version_node, "releaseTime");
    printf("version: %s\n", id->valuestring);
    printf("type: %s\n", type->valuestring);
    printf("release time: %s\n", releaseTime->valuestring);
    p->url =
        m_strdup(cJSON_GetObjectItem(target_version_node, "url")->valuestring);
    p->sha1 =
        m_strdup(cJSON_GetObjectItem(target_version_node, "sha1")->valuestring);
    p->path = m_strdup(".minecraft/versions/version.json");
    p->store = get_store_path(p->sha1);
  } else {
    printf("No version found\n");
  }
  cJSON_Delete(manifest);
  return p;
}
#endif
