//
// Created by root on 4/6/26.
//

#include "toml.h"

#include "utility/mtool.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <stdio.h>

//TODO 指定合理的函数名
/**
 *
 * @param file toml format file
 * @param tableName the table name,request"["and"]"
 * @param value want to add value eg."game = \"26.1.1\""
 */
void toml_add_on_table(FILE *file, const char *tableName, const char *value) {
  if (file == NULL) return;
  rewind(file);
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
      if (strncmp(line_start, tableName, strlen(tableName)) == 0) {
        f = 1;
      }
    } else if (f == 1 && line_start[0] == '[') {
      target = prev_pos;
      break;
    }
  }
  if (f == 0) {
    fseek(file, 0, SEEK_END);
    fputs(tableName, file);
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

void toml_delete_form_key(FILE *file,const char *key) {
  if (file == NULL) return;
  if (key == NULL) return;

  long pre_site = ftell(file);
  rewind(file);

  char *line = NULL;
  size_t len = 0;
  ssize_t read;

  long target = -1;
  while ((read = getline(&line, &len, file)) != -1) {
    size_t klen = strlen(key);
    if (strncmp(line, key, klen) == 0 && line[klen] == ' ') {
      target = ftell(file);
      break;
    }
  }
  if (target == -1) {
    free(line);
    fseek(file,pre_site,SEEK_SET);
    return;
  }
  fseek(file,0,SEEK_END);
  size_t size = ftell(file) - target;
  char *buffer = m_malloc(size + 1);
  fseek(file,target,SEEK_SET);
  fread(buffer,1,size,file);
  fseek(file,target-strlen(line),SEEK_SET);
  fwrite(buffer,1,size,file);
  ftruncate(fileno(file), target - strlen(line) + size);
  free(buffer);
  free(line);
  fseek(file,pre_site,SEEK_SET);
}

void toml_add_on_toptab(FILE *file,const char *key,const char *value) {
  if (file == NULL) return;
  long pre_site = ftell(file);
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  char *buffer = m_malloc(size + 1);
  rewind(file);
  fread(buffer,1,size,file);
  rewind(file);
  char *tmp;
  m_asprintf(&tmp,"%s = \"%s\"\n",key,value);
  fprintf(file,"%s",tmp);
  free(tmp);
  fwrite(buffer,1,size,file);
  free(buffer);
  fseek(file, pre_site, SEEK_SET);
}