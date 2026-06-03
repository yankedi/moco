//
// Created by dev on 6/3/26.
//

#include "modpack.h"

#include "cJSON.h"
#include "utility/file/zip.h"
#include "utility/mtool.h"
#include "utility/store/store.h"
#include "zip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * Parse a Modrinth .mrpack file and generate instance.toml + extract overrides.
 *
 * Requires: modpack_path points to a valid .mrpack (ZIP) archive containing
 *           modrinth.index.json with formatVersion=1 and game="minecraft".
 *
 * Does:
 *   - Validates formatVersion and game fields
 *   - Prompts if instance.toml already exists
 *   - Writes [game] + [dependencies] + [[mods]] sections to instance.toml
 *   - Extracts overrides/ directory from the .mrpack into .minecraft/
 *
 * @param modpack_path  path to the .mrpack file
 * @return 0 on success, 1 on failure (invalid format, I/O error, or user cancel)
 */
int parser_modrinth_modpack(const char *modpack_path) {
  int rc = 0;
  char *buffer = NULL;
  cJSON *json = NULL;
  FILE *file = NULL;
  char *line = NULL;
  struct zip_t *zip = NULL;

  buffer = zip_get_file(modpack_path, "modrinth.index.json");
  if (!buffer) { rc = 1; goto cleanup; }

  json = cJSON_Parse(buffer);
  if (!json) { rc = 1; goto cleanup; }

  int formatVersion = cJSON_GetObjectItemCaseSensitive(json, "formatVersion")->valueint;
  if (formatVersion != 1) {
    fprintf(stderr, "Unsupported formatVersion: %d\n", formatVersion);
    rc = 1; goto cleanup;
  }

  char *game = cJSON_GetObjectItemCaseSensitive(json, "game")->valuestring;
  if (strcmp(game, "minecraft") != 0) {
    fprintf(stderr, "Unsupported game: %s\n", game);
    rc = 1; goto cleanup;
  }

  if (access("instance.toml", F_OK) == 0) {
    size_t len = 0;
    printf("instance.toml already exists, Override? [y/N]: ");
    if (getline(&line, &len, stdin) == -1 || rpmatch(line) != 1) {
      rc = 0; goto cleanup;
    }
    remove("instance.toml");
  }
  char *slug = NULL;
  for (int i = strlen(modpack_path)-1;i >= 0;--i)
    if (modpack_path[i] == '/') slug = m_strdup(modpack_path + i + 1);
  slug[strlen(slug) - 5] = '\0';
  char *name = cJSON_GetObjectItemCaseSensitive(json, "name")->valuestring;
  char *versionId = cJSON_GetObjectItemCaseSensitive(json, "versionId")->valuestring;
  cJSON *deps = cJSON_GetObjectItemCaseSensitive(json, "dependencies");
  char *mc_ver = cJSON_GetObjectItemCaseSensitive(deps, "minecraft")->valuestring;

  file = fopen("instance.toml", "wb");
  if (!file) { rc = 1; goto cleanup; }
  fprintf(file, "[game]\n"
                "name = \"%s\"\n"
                "modpack_slug = \"%s\"\n"
                "modpack_version = \"%s\"\n"
                "modpack_mode = \"modrinth\"\n"
                "version = \"%s\"\n", name,slug, versionId, mc_ver);
  fprintf(file, "[dependencies]\n");
  free(slug);

  cJSON *forge = cJSON_GetObjectItemCaseSensitive(deps, "forge");
  cJSON *neoforge = cJSON_GetObjectItemCaseSensitive(deps, "neoforge");
  cJSON *fabric = cJSON_GetObjectItemCaseSensitive(deps, "fabric-loader");
  cJSON *quilt = cJSON_GetObjectItemCaseSensitive(deps, "quilt-loader");
  if (forge)         fprintf(file, "loader = \"forge\"\nloader_version = \"%s\"\n", forge->valuestring);
  else if (neoforge) fprintf(file, "loader = \"neoforge\"\nloader_version = \"%s\"\n", neoforge->valuestring);
  else if (fabric)   fprintf(file, "loader = \"fabric-loader\"\nloader_version = \"%s\"\n", fabric->valuestring);
  else if (quilt)    fprintf(file, "loader = \"quilt-loader\"\nloader_version = \"%s\"\n", quilt->valuestring);

  cJSON *mods = cJSON_GetObjectItemCaseSensitive(json, "files");
  cJSON *item;
  cJSON_ArrayForEach(item, mods) {
    cJSON *env = cJSON_GetObjectItem(item, "env");
    char *env_client = cJSON_GetObjectItem(env, "client")->valuestring;
    if (strcmp(env_client, "unsupported") != 0) {
      char *mod_path = cJSON_GetObjectItemCaseSensitive(item, "path")->valuestring;
      char *full_path = NULL;
      m_asprintf(&full_path, ".minecraft/%s", mod_path);
      char *sha1 = cJSON_GetObjectItemCaseSensitive(
          cJSON_GetObjectItemCaseSensitive(item, "hashes"), "sha1")->valuestring;
      char *url = cJSON_GetArrayItem(
          cJSON_GetObjectItemCaseSensitive(item, "downloads"), 0)->valuestring;
      if (full_path && sha1 && url) {
        fprintf(file, "[[mods]]\n"
                      "url = \"%s\"\n"
                      "sha1 = \"%s\"\n"
                      "path = \"%s\"\n", url, sha1, full_path);
      }
      free(full_path);
    }
  }

  fclose(file);
  file = NULL;

  zip = zip_open(modpack_path, 0, 'r');
  if (!zip) { rc = 1; goto cleanup; }

  size_t total = zip_entries_total(zip);
  for (int i = 0; i < (int)total; i++) {
    zip_entry_openbyindex(zip, i);
    const char *name_f = zip_entry_name(zip);
    if (strncmp(name_f, "overrides/", 10) == 0) {
      if (!zip_entry_isdir(zip)) {
        char *ov_path;
        m_asprintf(&ov_path, ".minecraft/%s", name_f + 10);
        mkdirs(ov_path, F);
        zip_entry_fread(zip, ov_path);
        free(ov_path);
      }
    }
    zip_entry_close(zip);
  }

cleanup:
  free(buffer);
  free(line);
  if (file) fclose(file);
  if (zip) zip_close(zip);
  cJSON_Delete(json);
  return rc;
}