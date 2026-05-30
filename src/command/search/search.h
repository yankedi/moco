//
// Created by root on 4/5/26.
//

#ifndef MOCO_SEARCH_H
#define MOCO_SEARCH_H
#include "interface.h"
typedef enum {V,L} search_mode;
extern void search(int argc, char *argv[]);
// extern Package *get_version(const char *v);
extern SearchResult *search_versions(const char *v);
extern SearchResult *search_version(const char *v);
extern SearchResult *search_forge(const char *id, const search_mode mode);
extern SearchResult *search_forges(const char *id, const search_mode mode);
extern SearchResult *search_neoforge(const char *id, const search_mode mode);
extern SearchResult *search_neoforges(const char *id, const search_mode mode);
extern SearchResult *search_fabric(const char *v);
extern SearchResult *search_mod(const char *slug);
extern SearchResult *search_mods(const char *slug);
extern SearchResult *search_modpack(const char *slug);
extern SearchResult *search_modpacks(const char *slug);
extern SearchResult *search_shader(const char *slug);
extern SearchResult *search_shaders(const char *slug);
#endif // MOCO_SEARCH_H
