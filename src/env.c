//
// Created by root on 4/6/26.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utility/mtool.h"

char *HOME = NULL;
char *XDG_DATA_HOME = NULL;
char *XDG_CONFIG_HOME = NULL;
char *XDG_CACHE_HOME = NULL;
char *XDG_STATE_HOME = NULL;

void env() {
  HOME = getenv("HOME");
  // TODO 不存在$HOME的情况
  // TODO XDG_XXX_XXX被设置的情况
  XDG_DATA_HOME = getenv("MOCO_HOME");
  if (XDG_DATA_HOME == NULL)
    m_asprintf(&XDG_DATA_HOME, "%s/.local/share/moco", HOME);
  else
    XDG_DATA_HOME = m_strdup(XDG_DATA_HOME);
  XDG_CONFIG_HOME = getenv("MOCO_CONFIG");
  if (XDG_CONFIG_HOME == NULL)
    m_asprintf(&XDG_CONFIG_HOME, "%s/.config/moco", HOME);
  else
    XDG_CONFIG_HOME = m_strdup(XDG_CONFIG_HOME);
  XDG_CACHE_HOME = getenv("MOCO_CACHE");
  if (XDG_CACHE_HOME == NULL)
    m_asprintf(&XDG_CACHE_HOME, "%s/.cache/moco", HOME);
  else
    XDG_CACHE_HOME = m_strdup(XDG_CACHE_HOME);
  XDG_STATE_HOME = getenv("MOCO_STATE");
  if (XDG_STATE_HOME == NULL)
    m_asprintf(&XDG_STATE_HOME, "%s/.local/state/moco", HOME);
  else
    XDG_STATE_HOME = m_strdup(XDG_STATE_HOME);
}
void free_env() {
  free(XDG_STATE_HOME);
  free(XDG_CACHE_HOME);
  free(XDG_DATA_HOME);
  free(XDG_CONFIG_HOME);
}
