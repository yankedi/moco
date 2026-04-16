#ifndef MOCO_MTOOL_H
#define MOCO_MTOOL_H
#include <stddef.h>
#include <stdio.h>
#include "interface.h"

char *m_strdup(const char *s);
int m_asprintf(char **strp, const char *fmt, ...);
extern void *m_malloc(size_t size);
extern char *m_sha1(FILE *fp);
extern char *m_replace(const char *target, m_string str);
#endif // MOCO_MTOOL_H
