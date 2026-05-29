//
// Created by dev on 5/6/26.
//

#include "version.h"

#include "env.h"
#include "m_exit.h"
#include "utility/file/json.h"
#include "utility/mtool.h"

#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
/*
static int mc_v(const char *v) {
  if (strncmp(v, "rd-", 3) == 0) { // Pre-classic   (2009-05)

  } else if (strncmp(v, "c0.", 3) == 0) { // Classic       (2009-05)

  } else if (strncmp(v, "inf-", 4) == 0) { // Infdev        (2010-02)

  } else if (strncmp(v, "a1.", 3) == 0) { // Alpha         (2010-06)

  } else if (strncmp(v, "b1.", 3) == 0) { // Beta          (2010-12)

  } else if (strstr(v, "pre") || strstr(v, "Pre")) { // pre / Pre-Release (2012+)

  } else if (strstr(v, "w")) { // Snapshots     (2013+)

  } else if (strstr(v, "combat")) { // Combat Test   (2019)

  } else if (strstr(v, "-rc")) { // Release Cand. (2020+)

  } else if (strstr(v, "snapshot")) { // New Snapshots (2025+)

  } else { // 正式版 (1.0, 1.20.4, 26.1 ...)

  }
}
*/

/**
 *
 * @param id a minecraft version string
 * @return 0 if version exists, -1 if not
 */
int has_version(const char *id) {
  char *json_path;
  m_asprintf(&json_path, "%s/version_manifest_v2.json", XDG_DATA_HOME);
  cJSON *json = file_to_json(json_path);
  free(json_path);
  cJSON *versions = cJSON_GetObjectItemCaseSensitive(json, "versions");

  /* Mojang strips trailing .0 from version strings (e.g. "26.1.0" is "26.1") */
  char *normalized = m_strdup(id);
  int nlen = strlen(normalized);
  while (nlen > 2 && normalized[nlen - 1] == '0' && normalized[nlen - 2] == '.') {
    normalized[nlen - 2] = '\0';
    nlen -= 2;
  }

  cJSON *tmp;
  cJSON_ArrayForEach(tmp, versions) {
    const cJSON *versionId = cJSON_GetObjectItemCaseSensitive(tmp, "id");
    if (strcmp(versionId->valuestring, normalized) == 0) {
      free(normalized);
      cJSON_Delete(json);
      return 0;
    }
  }
  free(normalized);
  cJSON_Delete(json);
  return -1;
}

/**
 *
 * @param v1 a minecraft version string
 * @param v2 same
 * @return 0 is same,1 is v1 > v2, -1 is v1 < v2
 */
int mc_version_compare(const char *v1, const char *v2) {
  if (strcmp(v1, v2) == 0) return 0;
  char *json_path;
  m_asprintf(&json_path, "%s/version_manifest_v2.json", XDG_DATA_HOME);
  if (access(json_path,F_OK) != 0) {
    fprintf(stderr, "Please run moco update first");
    free(json_path);
    m_exit(EX_DATAERR);
  }
  cJSON *json = file_to_json(json_path);
  free(json_path);
  cJSON *versions = cJSON_GetObjectItemCaseSensitive(json, "versions");
  cJSON *tmp;
  cJSON_ArrayForEach(tmp, versions) {
    const cJSON *versionId = cJSON_GetObjectItemCaseSensitive(tmp, "id");
    if (strcmp(versionId->valuestring, v1) == 0) {
      cJSON_Delete(json);
      return 1;
    }
    if (strcmp(versionId->valuestring, v2) == 0) {
      cJSON_Delete(json);
      return -1;
    }
  }
  cJSON_Delete(json);
  m_exit(EX_DATAERR);
}