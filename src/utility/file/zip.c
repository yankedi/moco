//
// Created by dev on 5/2/26.
//

#include "zip.h"
#include <zip.h>

#include "m_exit.h"
#include "utility/mtool.h"

#include <stdio.h>
#include <sysexits.h>
#include <unistd.h>
#include <zipconf.h>

char *zip_get_file(const char *zip_path, const char *file_name) {
  if (access(zip_path, F_OK) != 0) {
    fprintf(stderr, "File %s does not exist\n", zip_path);
    m_exit(EX_DATAERR);
  }

  int errorp;
  zip_t *archive = zip_open(zip_path, ZIP_RDONLY, &errorp);
  if (archive == NULL) {
    zip_error_t error;
    zip_error_init_with_code(&error, errorp);
    fprintf(stderr, "cannot open zip archive '%s': %s\n", zip_path, zip_error_strerror(&error));
    zip_error_fini(&error);
    m_exit(EX_DATAERR);
  }

  const zip_int64_t file_index = zip_name_locate(archive, file_name, ZIP_FL_NODIR | ZIP_FL_NOCASE);
  if (file_index == -1) {
    fprintf(stderr, "Unlocated file: %s/%s\n", zip_path, file_name);
    zip_close(archive);
    m_exit(EX_DATAERR);
  }

  zip_file_t *file_data = zip_fopen_index(archive, file_index, 0);
  if (file_data == NULL) {
    fprintf(stderr, "Unread the file:%s/%s", zip_path, file_name);
    zip_close(archive);
    m_exit(EX_DATAERR);
  }

  zip_stat_t stat;
  zip_stat_init(&stat);
  if (zip_stat_index(archive, file_index, 0, &stat) != 0) {
    fprintf(stderr, "Unread the file stat:%s/%s", zip_path, file_name);
    zip_fclose(file_data);
    zip_close(archive);
    m_exit(EX_DATAERR);
  }

  char *buffer = m_malloc(stat.size + 1);
  zip_int64_t bytes_read = zip_fread(file_data, buffer, stat.size);
  buffer[bytes_read] = '\0';

  zip_fclose(file_data);
  zip_close(archive);

  return buffer;
}