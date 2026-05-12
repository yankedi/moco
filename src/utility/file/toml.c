//
// Created by root on 4/6/26.
//

#include "toml.h"


#include <stdlib.h>
#include <string.h>

#include <stdio.h>

/**
 *
 * @param file toml format file
 * @param name the table name,request"["and"]"
 * @param value want to add value eg."game = \"26.1.1\""
 */
void toml_add_table(FILE *file, const char *name, const char *value) {
  if (file == NULL) return;
  char buffer[1024];
  char f = 0;
  long target = -1;
  long prev_pos = 0;
  while (1) {
    prev_pos = ftell(file);
    if (fgets(buffer, sizeof(buffer), file) == NULL) {
      if (f == 1) target = prev_pos;
      break;
    }
    char *line_start = buffer;
    while (*line_start == ' ' || *line_start == '\t') line_start++;
    if (f == 0) {
      if (strncmp(line_start, name, strlen(name)) == 0) {
        f = 1;
      }
    } else if (f == 1 && line_start[0] == '[') {
      target = prev_pos;
      break;
    }
  }
  if (f == 0) {
    fseek(file, 0, SEEK_END);
    fputs(name, file);
    fputs("\n", file);
    fputs(value, file);
    if (value[strlen(value) - 1] != '\n')
      fputs("\n", file);
    return;
  }
  if (target == -1) {
    target = prev_pos;
  }

  fseek(file, 0, SEEK_END);
  long size = ftell(file) - target;
  char *bsize= NULL;
  if (size > 0) {
    bsize = malloc(size + 1);
    if (bsize == NULL) return;
    fseek(file, target, SEEK_SET);
    fread(bsize, 1, size, file);
    bsize[size] = '\0';
  }
  fseek(file, target, SEEK_SET);
  fputs(value, file);
  if (value[strlen(value) - 1] != '\n') {
    fputs("\n", file);
  }
  if (size > 0) {
    fputs(bsize, file);
    free(bsize);
  }
}