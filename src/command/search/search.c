//
// Created by root on 4/5/26.
//

#include "search.h"
#include "cJSON.h"
#include "env.h"
#include "utility/file/json.h"
#include "utility/minecraft/version.h"
#include "utility/mtool.h"
#include "utility/store/store.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
    if (strcmp(subcommand, "versions") == 0) {
      SearchResult *result = search_versions(argv[optind + 1]);
      free_SearchResult(result);
    }
    if (strcmp(subcommand, "version") == 0) {
      SearchResult *result = search_version(argv[optind + 1]);
      free_SearchResult(result);
    }
    if (strcmp(subcommand,"forge") == 0) {
      SearchResult *result = search_forge(argv[optind + 1],V);
      free_SearchResult(result);
    }
    if (strcmp(subcommand,"forges") == 0) {
      SearchResult *result = search_forges(argv[optind + 1], V);
      free_SearchResult(result);
    }
    if (strcmp(subcommand,"neoforge") == 0) {
      SearchResult *result = search_neoforge(argv[optind + 1], V);
      free_SearchResult(result);
    }
    if (strcmp(subcommand,"neoforges") == 0) {
      SearchResult *result = search_neoforges(argv[optind + 1], V);
      free_SearchResult(result);
    }
    if (strcmp(subcommand,"fabric") == 0) {
      SearchResult *result = search_fabric(argv[optind + 1]);
      free_SearchResult(result);
    }
    if (strcmp(subcommand,"fabrics") == 0) {
      SearchResult *result = search_fabric("");
      free_SearchResult(result);
    }
  }
}

//TODO 超长文本分页显示

SearchResult *search_fabric(const char *id) {
  char *path;
  m_asprintf(&path, "%s/fabric_manifest.json", XDG_DATA_HOME);
  if (access(path,F_OK) !=0) {
    fprintf(stderr, "Please use the \"moco update\"command first.\n");
    free(path);
    return NULL;
  }
  cJSON *manifest = file_to_json(path);
  free(path);
  cJSON *loader = cJSON_GetObjectItemCaseSensitive(manifest, "loader");

  if (id != NULL && id[0] != '\0') {
    printf("Wow you choice fabric!\n"
           "But:\"Fabric Loader: a flexible, platform-independent loader of mods, primarily designed for Minecraft: Java Edition\"\n"
           "You can see:https://docs.fabricmc.net/develop/#what-does-fabric-offer\n");
    cJSON *item;
    cJSON_ArrayForEach(item, loader) {
      cJSON *ver = cJSON_GetObjectItemCaseSensitive(item, "version");
      if (strcmp(ver->valuestring, id) == 0) {
        SearchResult *result = m_malloc(sizeof(SearchResult));
        cJSON *v = cJSON_CreateObject();
        cJSON_AddStringToObject(v, "version", "fabric");
        cJSON_AddStringToObject(v, "loader", ver->valuestring);
        result->node = malloc(sizeof(cJSON *));
        result->node[0] = v;
        result->count = 1;
        cJSON_Delete(manifest);
        return result;
      }
    }
    cJSON_Delete(manifest);
    return NULL;
  }

  cJSON *version_res = cJSON_GetArrayItem(loader, 0);
  cJSON *version = cJSON_GetObjectItemCaseSensitive(version_res, "version");
  printf("Latest version is: %s\n", version->valuestring);
  SearchResult *result = m_malloc(sizeof(SearchResult));
  cJSON *v = cJSON_CreateObject();
  cJSON_AddStringToObject(v, "version", "fabric");
  cJSON_AddStringToObject(v, "loader", version->valuestring);
  result->node = malloc(sizeof(cJSON *));
  result->node[0] = v;
  result->count = 1;
  cJSON_Delete(manifest);
  return result;
}

SearchResult *search_neoforges(const char *id, const search_mode mode) {
  if (id == NULL) return NULL;
  char *path;
  m_asprintf(&path, "%s/neoforge_manifest.json", XDG_DATA_HOME);
  if (access(path, F_OK) != 0) {
    fprintf(stderr, "Please use the \"moco update\"command first.\n");
    free(path);
    return NULL;
  }
  cJSON *manifest = file_to_json(path);
  free(path);
  cJSON *versions = cJSON_GetObjectItemCaseSensitive(manifest, "versions");
  cJSON *version;
  SearchResult *result = m_malloc(sizeof(SearchResult));
  result->node = NULL;
  result->count = 0;
  int is_new = 0;
  char *id_t = NULL;
  if (mode == V) {
    is_new = (strncmp(id, "1.", 2) != 0);
    if (is_new) {
      id_t = m_strdup(id);
      // Strip trailing ".0" (e.g. "26.1.0" -> "26.1")
      { int l = strlen(id_t); while (l > 2 && id_t[l-1] == '0' && id_t[l-2] == '.') { id_t[l-2] = '\0'; l -= 2; } }
    } else id_t = m_strdup(id + 2);
  }
  cJSON_ArrayForEach(version, versions) {
    char *version_n = m_strdup(version->valuestring);
    char *p = strchr(version_n, '.');
    p = strchr(p + 1, '.');
    int v_is_new;
    if (mode == V) v_is_new = is_new;
    else v_is_new = (strtol(version_n, NULL, 10) >= 26);
    if (v_is_new) p = strchr(p + 1, '.');
    if (p == NULL) {
      free(version_n);
      continue;
    }

    char *f = strndup(version_n, p - version_n);
    char *b = m_strdup(p + 1);
    // Strip trailing ".0" to match Mojang naming (e.g. "26.1.0" -> "26.1")
    { int fl = strlen(f); while (fl > 2 && f[fl-1] == '0' && f[fl-2] == '.') { f[fl-2] = '\0'; fl -= 2; } }

    const char *target = (mode == V) ? f : version_n;
    const char *pattern = (mode == V) ? id_t : id;
    if (strstr(target, pattern) != NULL) {
      cJSON *v = cJSON_CreateObject();
      if (!v_is_new) {
        char *full_f = NULL;
        m_asprintf(&full_f, "1.%s", f);
        cJSON_AddStringToObject(v, "version", full_f);
        free(full_f);
      } else {
        cJSON_AddStringToObject(v, "version", f);
      }
      cJSON_AddStringToObject(v, "loader", version_n);
      result->node = realloc(result->node, sizeof(cJSON *) * (result->count + 1));
      memmove(result->node + 1, result->node, sizeof(cJSON *) * result->count);
      result->node[0] = v;
      result->count++;
    }
    free(f);
    free(b);
    free(version_n);
  }
  free(id_t);
  if (result->count != 0) {
    for (int i = 0; i < result->count; ++i) {
      cJSON *node = result->node[i];
      printf("minecraft version: %s\n", cJSON_GetObjectItemCaseSensitive(node, "version")->valuestring);
      printf("neoforge version: %s\n\n", cJSON_GetObjectItemCaseSensitive(node, "loader")->valuestring);
    }
    cJSON_Delete(manifest);
    return result;
  }
  free(result);
  cJSON_Delete(manifest);
  return NULL;
}

SearchResult *search_neoforge(const char *id, const search_mode mode) {
  if (id == NULL) return NULL;
  char *path;
  m_asprintf(&path, "%s/neoforge_manifest.json", XDG_DATA_HOME);
  if (access(path, F_OK) != 0) {
    fprintf(stderr, "Please use the \"moco update\"command first.\n");
    free(path);
    return NULL;
  }
  cJSON *manifest = file_to_json(path);
  free(path);
  if (manifest == NULL) {
    fprintf(stderr, "Failed to parse neoforge manifest.\n");
    return NULL;
  }
  cJSON *versions = cJSON_GetObjectItemCaseSensitive(manifest, "versions");
  cJSON *version;
  SearchResult *result = m_malloc(sizeof(SearchResult));
  result->node = NULL;
  result->count = 0;
  int is_new = 0;
  char *id_t = NULL;
  if (mode == V) {
    is_new = (strncmp(id, "1.", 2) != 0);
    if (is_new) {
      id_t = m_strdup(id);
      // Strip trailing ".0" (e.g. "26.1.0" -> "26.1")
      { int l = strlen(id_t); while (l > 2 && id_t[l-1] == '0' && id_t[l-2] == '.') { id_t[l-2] = '\0'; l -= 2; } }
    } else id_t = m_strdup(id + 2);
  }
  cJSON_ArrayForEach(version, versions) {
    char *version_n = m_strdup(version->valuestring);
    char *p = strchr(version_n, '.');
    p = strchr(p + 1, '.');
    int v_is_new;
    if (mode == V) v_is_new = is_new;
    else v_is_new = (strtol(version_n, NULL, 10) >= 26);
    if (v_is_new) p = strchr(p + 1, '.');
    if (p == NULL) {
      free(version_n);
      continue;
    }

    char *f = strndup(version_n, p - version_n);
    char *b = m_strdup(p + 1);
    // Strip trailing ".0" to match Mojang naming (e.g. "26.1.0" -> "26.1")
    { int fl = strlen(f); while (fl > 2 && f[fl-1] == '0' && f[fl-2] == '.') { f[fl-2] = '\0'; fl -= 2; } }

    const char *target = (mode == V) ? f : version_n;
    const char *pattern = (mode == V) ? id_t : id;
    if (strcmp(target, pattern) == 0) {
      cJSON *v = cJSON_CreateObject();
      if (!v_is_new) {
        char *full_f = NULL;
        m_asprintf(&full_f, "1.%s", f);
        cJSON_AddStringToObject(v, "version", full_f);
        free(full_f);
      } else {
        cJSON_AddStringToObject(v, "version", f);
      }
      cJSON_AddStringToObject(v, "loader", version_n);
      result->node = realloc(result->node, sizeof(cJSON *) * (result->count + 1));
      memmove(result->node + 1, result->node, sizeof(cJSON *) * result->count);
      result->node[0] = v;
      result->count++;
    }
    free(f);
    free(b);
    free(version_n);
  }
  free(id_t);
  if (result->count != 0) {
    for (int i = 0; i < result->count; ++i) {
      cJSON *node = result->node[i];
      printf("minecraft version: %s\n", cJSON_GetObjectItemCaseSensitive(node, "version")->valuestring);
      printf("neoforge version: %s\n\n", cJSON_GetObjectItemCaseSensitive(node, "loader")->valuestring);
    }
    cJSON_Delete(manifest);
    return result;
  }
  free(result);
  cJSON_Delete(manifest);
  return NULL;
}

SearchResult *search_forges(const char *id, const search_mode mode) {
  if (id == NULL) return NULL;
  if (mode == V && has_version(id) != 0) {
    fprintf(stderr, "Minecraft version %s not found\n", id);
    return NULL;
  }
  char *path;
  m_asprintf(&path, "%s/forge_manifest.json", XDG_DATA_HOME);
  if (access(path, F_OK) != 0) {
    fprintf(stderr, "Please use the \"moco update\"command first.\n");
    free(path);
    return NULL;
  }
  cJSON *manifest = file_to_json(path);
  free(path);
  cJSON *versions = cJSON_GetObjectItemCaseSensitive(
  cJSON_GetObjectItemCaseSensitive(
    cJSON_GetObjectItemCaseSensitive(
      cJSON_GetObjectItemCaseSensitive(manifest,"metadata"),"versioning"),"versions"), "version");
  cJSON *version;
  SearchResult *result = m_malloc(sizeof(SearchResult));
  result->node = NULL;
  result->count = 0;
  cJSON_ArrayForEach(version, versions) {
    char *version_n = m_strdup(version->valuestring);
    char *f = strtok(version_n, "-");
    char *b = strtok(NULL,"-");
    const char *target = mode == V ? f : b;
    if (strstr(target, id) != NULL) {
      cJSON *v = cJSON_CreateObject();
      cJSON_AddStringToObject(v, "version", f);
      cJSON_AddStringToObject(v, "loader", b);
      result->node = realloc(result->node, sizeof(cJSON *) * (result->count + 1));
      result->node[result->count] = v;
      result->count++;
      printf("minecraft version: %s\n", f);
      printf("forge version: %s\n\n", b);
    }
    free(version_n);
  }
  if (result->count != 0) {
    cJSON_Delete(manifest);
    return result;
  }
  free(result);
  cJSON_Delete(manifest);
  return NULL;
}

SearchResult *search_forge(const char *id,const search_mode mode) {
  if (id == NULL) return NULL;
  if (mode == V &&has_version(id)!=0) {
    fprintf(stderr, "Minecraft version %s not found\n", id);
    return NULL;
  }
  char *path;
  m_asprintf(&path, "%s/forge_manifest.json", XDG_DATA_HOME);
  if (access(path, F_OK) != 0) {
    fprintf(stderr, "Please use the \"moco update\"command first.\n");
    free(path);
    return NULL;
  }
  cJSON *manifest = file_to_json(path);
  free(path);
  if (manifest == NULL) {
    fprintf(stderr, "Failed to parse forge manifest.\n");
    return NULL;
  }
  cJSON *versions = cJSON_GetObjectItemCaseSensitive(
  cJSON_GetObjectItemCaseSensitive(
    cJSON_GetObjectItemCaseSensitive(
      cJSON_GetObjectItemCaseSensitive(manifest,"metadata"),"versioning"),"versions"), "version");
  cJSON *version;
  SearchResult *result = m_malloc(sizeof(SearchResult));
  result->node = NULL;
  result->count = 0;
  cJSON_ArrayForEach(version, versions) {
    char *version_n = m_strdup(version->valuestring);
    char *f = strtok(version_n, "-");
    char *b = strtok(NULL,"-");
    int compare;
    if (mode == V) compare = strcmp(f, id);
    else compare = strcmp(b, id);
    if (compare == 0) {
      cJSON *v = cJSON_CreateObject();
      cJSON_AddStringToObject(v, "version", f);
      cJSON_AddStringToObject(v, "loader", b);
      result->node = realloc(result->node, sizeof(cJSON *) * (result->count + 1));
      result->node[result->count] = v;
      result->count++;
      printf("minecraft version: %s\n", f);
      printf("forge version: %s\n\n", b);
    }
    free(version_n);
  }
  if (result->count != 0) {
    cJSON_Delete(manifest);
    return result;
  }
  free(result);
  cJSON_Delete(manifest);
  return NULL;
}
SearchResult *search_versions(const char *v) {
  if (v == NULL) return NULL;
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
  if (v == NULL) return NULL;
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
  free(result);
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
