//
// Created by root on 4/6/26.
//

#ifndef MOCO_TOML_H
#define MOCO_TOML_H
#include "tomlc17.h"
extern void toml_add_on_table(FILE *file, const char *tableName, const char *value);
extern void toml_delete_form_key(FILE *file,const char *key);
extern void toml_add_on_toptab(FILE *file,const char *key,const char *value);
#endif // MOCO_TOML_H
