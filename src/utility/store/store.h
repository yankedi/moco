//
// Created by root on 4/5/26.
//

#ifndef MOCO_STORE_H
#define MOCO_STORE_H
// extern char *getPath(package *p);
typedef enum { R = 0, F = 1 } mkdir_mode;
extern void mkdirs(const char *path, mkdir_mode mode);
extern void link_mkdir(const char *store, const char *path);
extern char *get_store_path(const char *sha1);
extern char *get_tmp(const char *store);
#endif // MOCO_STORE_H
