//
// Created by root on 4/4/26.
//

#ifndef MOCO_UPDATE_H
#define MOCO_UPDATE_H
/*
typedef enum { SOURCE_OFFICIAL = 0, SOURCE_BMCLAPI = 1 } SourceType;

typedef struct {
  const char *name;
  const char *version_manifest;
  const char *assets;
} Source;

extern const Source SOURCES[];
extern SourceType current_source;
*/
extern void update(void);
#endif // MOCO_UPDATE_H
