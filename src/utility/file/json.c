//
// Created by root on 4/5/26.
//

#include "json.h"
#include "cJSON.h"
#include "utility/mtool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * Read the whole file at `path` into a newly allocated, NUL-terminated buffer.
 *
 * Returns:
 * - Pointer to heap memory owned by caller (must be freed with free()) on
 * success.
 * - NULL if the file cannot be opened, size cannot be determined, memory
 * allocation fails, or a read I/O error occurs.
 */
cJSON *file_to_json(const char *path) {
  char *buffer = NULL;
  FILE *fp = NULL;
  if (access(path, F_OK) != 0) {
    fprintf(stderr, "file:\"%s\" no found\n", path);
    free(buffer);
    return NULL;
  }
  fp = fopen(path, "rb");
  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);

  if (size == -1L) {
    fprintf(stderr, "Failed to get file size for %s\n", path);
    fclose(fp);
    return NULL;
  }
  size_t fsize = (size_t)size;
  fseek(fp, 0, SEEK_SET);

  buffer = m_malloc(fsize + 1);
  if (fsize > 0) {
    size_t read_size = fread(buffer, 1, fsize, fp);
    if (read_size < fsize) {
      fprintf(stderr, "In reading \"%s\" I/O Error!\n", path);
      free(buffer);
      fclose(fp);
      return NULL;
    }
  }
  buffer[fsize] = '\0';
  fclose(fp);
  cJSON *json = cJSON_Parse(buffer);
  free(buffer);
  return json;
}
