//
// Created by dev on 5/13/26.
//

#ifndef MOCO_MODRINTH_SEARCH_H
#define MOCO_MODRINTH_SEARCH_H
#include "interface.h"
extern void search_modrinth(int argc, char *argv[]);
extern SearchResult *modrinth_search_mod(const char *slug, const char *version, const char *loader);
extern SearchResult *modrinth_search_mods(const char *slug, const char *version, const char *loader);
extern SearchResult *modrinth_search_modpack(const char *slug, const char *version, const char *loader);
extern SearchResult *modrinth_search_shader(const char *slug, const char *version, const char *loader);
#endif //MOCO_MODRINTH_SEARCH_H
