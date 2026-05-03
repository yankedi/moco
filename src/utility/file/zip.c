//
// Created by dev on 5/2/26.
//

#include "zip.h"
#include <zip.h>

#include "m_exit.h"

#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

char *zip_get_file(const char *zip_path, const char *file_name) {
  if (access(zip_path, F_OK) != 0) {
    fprintf(stderr, "File %s does not exist\n", zip_path);
    m_exit(EX_DATAERR);
  }

  struct zip_t *archive = zip_open(zip_path, 0, 'r');
  if (archive == NULL) {
    fprintf(stderr, "cannot open zip archive '%s'\n", zip_path);
    m_exit(EX_DATAERR);
  }

  if (zip_entry_open(archive, file_name) != 0) {
    fprintf(stderr, "Unlocated file: %s/%s\n", zip_path, file_name);
    zip_close(archive);
    m_exit(EX_DATAERR);
  }

  void *buffer = NULL;
  size_t bufsize = 0;
  ssize_t bytes_read = zip_entry_read(archive, &buffer, &bufsize);
  if (bytes_read < 0) {
    fprintf(stderr, "Unread the file: %s/%s\n", zip_path, file_name);
    free(buffer);
    zip_entry_close(archive);
    zip_close(archive);
    m_exit(EX_DATAERR);
  }

  zip_entry_close(archive);
  zip_close(archive);

  buffer = realloc(buffer, (size_t)bytes_read + 1);
  ((char *)buffer)[bytes_read] = '\0';
  return buffer;
}
