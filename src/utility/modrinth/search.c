//
// Created by dev on 5/13/26.
//

#include "search.h"

#include "interface.h"
#include "m_exit.h"

#include "utility/minecraft/version.h"
#include "command/login/curl.h"
#include "utility/mtool.h"
#include "notcurses/notcurses.h"
#include "tui/style.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

// https://api.modrinth.com/v3/search

static void init(void);
static void cleanup(void);
static void deal_new_filter(void);
static void tui_search_result_show(cJSON *hits, int total, const char *query, const char *sort, const char *loader, char **mc_ver, int mc_ver_count, const char *ptype, char **cats, int cats_count, const char *author);

static char *query;
#define LIMIT 100
#define OFFSET 0
static char *sort;
static char *new_filters;

static char *project_types;
static char **categories;
static int categories_count;
static char *loaders;
static char **game_versions;
static int game_versions_count;
static char *author;

static const char *category_mod[] = {
    "adventure, cursed, decoration, economy, equipment, food, game-mechanics, magic, mobs, social, storage, transportation, utility, worldgen, library, optimization, technology",
    "adventure",
    "cursed",
    "decoration",
    "economy",
    "equipment",
    "food",
    "game-mechanics",
    "magic",
    "mobs",
    "social",
    "storage",
    "transportation",
    "utility",
    "worldgen",
    "library",
    "optimization",
    "technology"};
static const char *category_modpack[] = {
    "adventure, challenging, combat, kitchen-sink, lightweight, magic, multiplayer, optimization, quests, technology",
    "adventure",
    "challenging",
    "combat",
    "kitchen-sink",
    "lightweight",
    "magic",
    "multiplayer",
    "optimization",
    "quests",
    "technology"};
static const char *category_resourcepack[] = {
    "16x, 32x, 48x, 64x, 128x, 256x, 512x-and-higher, cartoon, fantasy, medieval, modern, realistic, vanilla-like",
    "16x",
    "32x",
    "48x",
    "64x",
    "128x",
    "256x",
    "512x-and-higher",
    "cartoon",
    "fantasy",
    "medieval",
    "modern",
    "realistic",
    "vanilla-like"};

void search_modrinth(int argc, char *argv[]) {
  init();
  int opt;
  int option_index = 0;
  optind = 1;
  // capture query before options so it doesn't block getopt
  if (optind < argc && argv[optind] && argv[optind][0] != '-') {
    free(query);
    query = m_strdup(argv[optind]);
    optind++;
  }
  while ((opt = getopt_long(argc, argv, "hv:l:s:t:c:a:", search_modrinth_options, &option_index)) != -1) {
    switch (opt) {
    case 'h':
      printf("Usage: moco search <query> [options]\n");
      printf("Options:\n");
      printf("  -h, --help    Show this help message\n");
      printf("  -v, --version Filter by Minecraft version\n");
      printf("  -l, --loader  Filter by modloader\n");
      printf("  -s, --sort    Sort by relevance,downloads,follows,newest,updated\n");
      printf("  -t, --type     Filter by mod,modpack,resourcepack,shader,datapack,plugin\n");
      printf("  -c, --categories Filter by category\n");
      printf("  -a, --author    Filter by author\n");
      break;
    case 'v':;
      char *v = m_strdup(optarg);
      int vlen = (int)strlen(v);
      while (vlen > 0 && v[vlen - 1] == '0') v[--vlen] = '\0';
      if (vlen > 0 && v[vlen - 1] == '.') v[--vlen] = '\0';
      if (has_version(v) != 0) {
        fprintf(stderr, "Minecraft version %s not found\n", v);
        free(v);
        cleanup();
        m_exit(EX_DATAERR);
      }
      game_versions = realloc(game_versions, (game_versions_count + 1) * sizeof(char *));
      game_versions[game_versions_count++] = v;
      break;
    case 'l':
      if (
          strcmp(optarg, "forge") == 0 ||
          strcmp(optarg, "neoforge") == 0 ||
          strcmp(optarg, "fabric") == 0) {
        free(loaders);
        loaders = m_strdup(optarg);
      } else {
        fprintf(stderr, "loader: %s not found or unsupported\n", optarg);
        cleanup();
        m_exit(EX_DATAERR);
      }
      break;
    case 's':
      if (
          strcmp(optarg, "relevance") == 0 ||
          strcmp(optarg, "downloads") == 0 ||
          strcmp(optarg, "follows") == 0 ||
          strcmp(optarg, "newest") == 0 ||
          strcmp(optarg, "updated") == 0) {
        free(sort);
        sort = m_strdup(optarg);
      } else {
        fprintf(stderr, "sort: %s not found or unsupported\n", optarg);
        cleanup();
        m_exit(EX_DATAERR);
      }
      break;
    case 't':
      if (
          strcmp(optarg, "mod") == 0 ||
          strcmp(optarg, "modpack") == 0 ||
          strcmp(optarg, "resourcepack") == 0 ||
          strcmp(optarg, "shader") == 0 ||
          strcmp(optarg, "datapack") == 0 ||
          strcmp(optarg, "plugin") == 0) {
        free(project_types);
        project_types = m_strdup(optarg);
      } else {
        fprintf(stderr, "type: %s not found or unsupported\n", optarg);
        cleanup();
        m_exit(EX_DATAERR);
      }
      break;
    case 'c':
      char flag = 0;
      for (int i = 1; i < sizeof(category_mod) / sizeof(category_mod[0]); ++i) {
        if (strcmp(optarg, category_mod[i]) == 0) {
          flag = 1;
          break;
        }
      }
      for (int i = 1; i < sizeof(category_modpack) / sizeof(category_modpack[0]) && flag == 0; ++i) {
        if (strcmp(optarg, category_modpack[i]) == 0) {
          flag = 1;
          break;
        }
      }
      for (int i = 1; i < sizeof(category_resourcepack) / sizeof(category_resourcepack[0]) && flag == 0; ++i) {
        if (strcmp(optarg, category_resourcepack[i]) == 0) {
          flag = 1;
          break;
        }
      }
      if (flag == 1) {
        categories = realloc(categories, (categories_count + 1) * sizeof(char *));
        categories[categories_count++] = m_strdup(optarg);
        break;
      }else {
        fprintf(stderr, "categories: %s not found or unsupported\n", optarg);
        printf("mod categories: %s\n", category_mod[0]);
        printf("modpack categories: %s\n", category_modpack[0]);
        printf("resourcepack categories: %s\n", category_resourcepack[0]);
        cleanup();
        m_exit(EX_DATAERR);
      }
      break;
    case 'a':
      free(author);
      author = m_strdup(optarg);
      break;
    default:
      fprintf(stderr, "Unknown option: %c\n", opt);
    }
  }

  deal_new_filter();
  char *eq = curl_encode(query);
  char *url;
  m_asprintf(&url, "https://api.modrinth.com/v3/search?query=%s&index=%s%s&limit=%d&offset=%d",
             eq, sort, new_filters, LIMIT, OFFSET);
  free(eq);
  char *response;
  curl_get(url, &response);
  cJSON *json = cJSON_Parse(response);
  free(response);
  cJSON *hits_array = cJSON_GetObjectItemCaseSensitive(json, "hits");
  int total_hits = hits_array ? cJSON_GetArraySize(hits_array) : 0;
  if (total_hits == 0) {
    cJSON_Delete(json);
    printf("No results found.\n");
    cleanup();
    free(new_filters);
    free(url);
    return;
  }

  tui_search_result_show(hits_array, total_hits, query, sort, loaders, game_versions, game_versions_count, project_types, categories, categories_count, author);

  cJSON_Delete(json);
  free(new_filters);
  free(url);
}

static char *join_cjson_array(const cJSON *arr, int dedup) {
  if (!cJSON_IsArray(arr) || cJSON_GetArraySize(arr) == 0)
    return m_strdup("");
  const char *seen[32];
  int n = 0;
  const cJSON *item;
  char *result = m_strdup("");
  cJSON_ArrayForEach(item, arr) {
    if (!cJSON_IsString(item) || !item->valuestring) continue;
    if (dedup) {
      int found = 0;
      for (int i = 0; i < n; i++) {
        if (strcmp(seen[i], item->valuestring) == 0) { found = 1; break; }
      }
      if (found) continue;
      if (n < 32) seen[n++] = item->valuestring;
    }
    char *tmp = result;
    if (result[0])
      m_asprintf(&result, "%s, %s", tmp, item->valuestring);
    else
      m_asprintf(&result, "%s", item->valuestring);
    free(tmp);
  }
  return result;
}

struct tui_lay {
  int left_w, right_w, list_h, vis_rows;
  struct ncplane *hdr, *left, *right, *footer, *ver_sub, *sum_sub;
};

static void tui_resize(struct notcurses *nc, struct tui_lay *L,
                       int hdr_h, int footer_h, int row_h) {
  unsigned dimy, dimx;
  notcurses_stddim_yx(nc, &dimy, &dimx);
  int new_lw = (int)dimx * 5 / 16, new_rw = (int)dimx - new_lw;
  int new_lh = (int)dimy - hdr_h - footer_h;
  if (new_rw == L->right_w && new_lh == L->list_h) return;

  L->left_w = new_lw; L->right_w = new_rw; L->list_h = new_lh;
  L->vis_rows = new_lh / row_h;
  ncplane_resize(L->hdr,    0, 0, 0, 0, 0, 0, hdr_h, dimx);
  ncplane_resize(L->left,   0, 0, 0, 0, 0, 0, new_lh, new_lw);
  ncplane_resize(L->right,  0, 0, 0, 0, 0, 0, new_lh, new_rw);
  ncplane_move_yx(L->right, hdr_h, new_lw);
  ncplane_resize(L->footer, 0, 0, 0, 0, 0, 0, footer_h, dimx);
  ncplane_move_yx(L->footer, (int)dimy - footer_h, 0);
  ncplane_resize(L->ver_sub, 0, 0, 0, 0, 0, 0, 4, new_rw - 12);
  ncplane_resize(L->sum_sub, 0, 0, 0, 0, 0, 0, 4, new_rw - 12);
}

static void tui_render_hdr(struct ncplane *hdr, int total_hits,
    const char *query, const char *sort,
    const char *loaders, char **gv, int gv_n, const char *ptype,
    char **cats, int cats_n, const char *author) {
  ncplane_erase(hdr);
  ncplane_set_fg_rgb8(hdr, TUI_HDR_FG);
  ncplane_printf_yx(hdr, 0, 2, "%s  %d results  sort: %s",
    query[0] ? query : "(all)", total_hits, sort);

  char *fl = m_strdup("");
  int sp = 0;
  if (loaders && loaders[0]) {
    char *old = fl;
    m_asprintf(&fl, "%s%sloader: %s", old, sp ? "  |  " : "", loaders);
    free(old); sp = 1;
  }
  if (gv_n > 0) {
    char *old = fl, *j = m_strdup(gv[0]);
    for (int i = 1; i < gv_n; i++) {
      char *t = j;
      m_asprintf(&j, "%s, %s", t, gv[i]);
      free(t);
    }
    m_asprintf(&fl, "%s%smc: %s", old, sp ? "  |  " : "", j);
    free(old); free(j); sp = 1;
  }
  if (ptype && ptype[0]) {
    char *old = fl;
    m_asprintf(&fl, "%s%stype: %s", old, sp ? "  |  " : "", ptype);
    free(old); sp = 1;
  }
  if (cats_n > 0) {
    char *old = fl, *j = m_strdup(cats[0]);
    for (int i = 1; i < cats_n; i++) {
      char *t = j;
      m_asprintf(&j, "%s, %s", t, cats[i]);
      free(t);
    }
    m_asprintf(&fl, "%s%scategories: %s", old, sp ? "  |  " : "", j);
    free(old); free(j); sp = 1;
  }
  if (author && author[0]) {
    char *old = fl;
    m_asprintf(&fl, "%s%sauthor: %s", old, sp ? "  |  " : "", author);
    free(old); sp = 1;
  }
  if (fl[0]) ncplane_printf_yx(hdr, 1, 2, "%s", fl);
  free(fl);
}

static void tui_render_list(struct ncplane *left, cJSON *hits, int total,
    int lw, int row_h, int vis_rows, int *sel, int *sco) {
  ncplane_erase(left);
  if (*sel >= total) *sel = total - 1;
  if (*sel < 0) *sel = 0;
  if (*sel < *sco) *sco = *sel;
  if (*sel >= *sco + vis_rows) *sco = *sel - vis_rows + 1;
  if (*sco < 0) *sco = 0;

  for (int i = *sco; i < *sco + vis_rows && i < total; i++) {
    cJSON *mod = cJSON_GetArrayItem(hits, i);
    cJSON *nm = cJSON_GetObjectItemCaseSensitive(mod, "name");
    cJSON *au = cJSON_GetObjectItemCaseSensitive(mod, "author");
    int y = (i - *sco) * row_h;
    int is_sel = (i == *sel);
    if (is_sel) {
      ncplane_set_fg_rgb8(left, TUI_LIST_SEL_FG);
      ncplane_on_styles(left, NCSTYLE_BOLD);
    } else {
      ncplane_set_fg_rgb8(left, TUI_LIST_FG);
      ncplane_off_styles(left, NCSTYLE_BOLD);
    }
    cJSON *pt = cJSON_GetObjectItemCaseSensitive(mod, "project_types");
    char ptype[32] = "";
    if (pt && cJSON_GetArraySize(pt) > 0)
      snprintf(ptype, sizeof(ptype), "[%s]", cJSON_GetArrayItem(pt, 0)->valuestring);
    int plen = (int)strlen(ptype);
    int name_max = lw - plen - 8;
    if (name_max < 4) name_max = 4;
    int name_len = (int)strlen(nm->valuestring);
    if (name_len > name_max)
      ncplane_printf_yx(left, y, 0, " %c %-3d %.*s...",
        is_sel ? '>' : ' ', i + 1, name_max - 3, nm->valuestring);
    else
      ncplane_printf_yx(left, y, 0, " %c %-3d %s",
        is_sel ? '>' : ' ', i + 1, nm->valuestring);
    if (plen) ncplane_printf_yx(left, y, lw - plen - 1, "%s", ptype);
    ncplane_set_fg_rgb8(left, TUI_LIST_AUTHOR_FG);
    ncplane_off_styles(left, NCSTYLE_BOLD);
    ncplane_printf_yx(left, y + 1, 7, "%s", au->valuestring);
  }
}

static int tui_render_wrapped(struct ncplane *sub, struct ncplane *right,
    int rw, int max_h, int y, const char *label, const char *text, int fg[3]) {
  ncplane_set_fg_rgb8(right, fg[0], fg[1], fg[2]);
  ncplane_printf_yx(right, y, 2, "%s", label);
  int sw = rw - 12;
  ncplane_resize(sub, 0, 0, 0, 0, 0, 0, max_h, sw);
  ncplane_move_yx(sub, y, 12);
  ncplane_erase(sub);
  ncplane_set_fg_rgb8(sub, TUI_DETAIL_TAG_VAL_FG);
  ncplane_puttext(sub, 0, NCALIGN_LEFT, text, NULL);
  unsigned ey, ex;
  ncplane_cursor_yx(sub, &ey, &ex);
  int h = (int)ey + 1;
  ncplane_resize(sub, 0, 0, h, sw, 0, 0, h, sw);
  return h;
}

static void tui_render_detail(struct ncplane *right, struct ncplane *ver_sub,
    struct ncplane *sum_sub, cJSON *hits, int sel, int rw, int lh) {
  ncplane_erase(right);
  if (sel < 0) return;
  cJSON *mod = cJSON_GetArrayItem(hits, sel);
  cJSON *nm = cJSON_GetObjectItemCaseSensitive(mod, "name");
  cJSON *sl = cJSON_GetObjectItemCaseSensitive(mod, "slug");
  cJSON *au = cJSON_GetObjectItemCaseSensitive(mod, "author");
  cJSON *og = cJSON_GetObjectItemCaseSensitive(mod, "organization");
  cJSON *li = cJSON_GetObjectItemCaseSensitive(mod, "license");
  cJSON *dl = cJSON_GetObjectItemCaseSensitive(mod, "downloads");
  cJSON *fw = cJSON_GetObjectItemCaseSensitive(mod, "follows");
  cJSON *dc = cJSON_GetObjectItemCaseSensitive(mod, "date_created");
  cJSON *dm = cJSON_GetObjectItemCaseSensitive(mod, "date_modified");
  int ry = 1;

  ncplane_set_fg_rgb8(right, TUI_DETAIL_TITLE_FG);
  ncplane_on_styles(right, NCSTYLE_BOLD);
  ncplane_printf_yx(right, ry++, 2, "%s", nm->valuestring);
  ncplane_off_styles(right, NCSTYLE_BOLD);
  ncplane_set_fg_rgb8(right, TUI_DETAIL_SLUG_FG);
  ncplane_printf_yx(right, ry++, 2, "%s", sl->valuestring);
  ncplane_set_fg_rgb8(right, TUI_DETAIL_SEP_FG);
  ncplane_printf_yx(right, ry++, 2, "─────────────────────────────────────────");
  ncplane_set_fg_rgb8(right, TUI_DETAIL_AUTHOR_FG);
  ncplane_printf_yx(right, ry++, 2, "Author:  %s%s%s",
    au->valuestring, og && og->valuestring ? " / " : "",
    og && og->valuestring ? og->valuestring : "");
  if (li && li->valuestring) {
    ncplane_set_fg_rgb8(right, TUI_DETAIL_LICENSE_FG);
    ncplane_printf_yx(right, ry++, 2, "License: %s", li->valuestring);
  }
  ncplane_set_fg_rgb8(right, TUI_DETAIL_STATS_FG);
  ncplane_printf_yx(right, ry++, 2, "Downloads: %d  |  Follows: %d",
    dl ? dl->valueint : 0, fw ? fw->valueint : 0);

  cJSON *sm = cJSON_GetObjectItemCaseSensitive(mod, "summary");
  const char *smt = cJSON_IsString(sm) ? sm->valuestring : "";
  if (smt[0]) {
    int sf[3] = {TUI_DETAIL_SUMMARY_FG};
    ry += tui_render_wrapped(sum_sub, right, rw, lh - ry, ry, "Summary: ", smt, sf);
  }
  if (dc && dc->valuestring) {
    ncplane_set_fg_rgb8(right, 80, 80, 80);
    ncplane_printf_yx(right, ry++, 2, "Created:  %.10s", dc->valuestring);
  }
  if (dm && dm->valuestring) {
    ncplane_set_fg_rgb8(right, 80, 80, 80);
    ncplane_printf_yx(right, ry++, 2, "Updated:  %.10s", dm->valuestring);
  }
  cJSON *ld = cJSON_GetObjectItemCaseSensitive(mod, "loaders");
  char *lds = join_cjson_array(ld, 1);
  ncplane_set_fg_rgb8(right, TUI_DETAIL_LOADERS_FG);
  ncplane_printf_yx(right, ry++, 2, "Loaders:  %s", lds[0] ? lds : "N/A");
  free(lds);

  cJSON *gvj = cJSON_GetObjectItemCaseSensitive(mod, "game_versions");
  char *gvs = join_cjson_array(gvj, 0);
  if (gvs[0]) {
    int vf[3] = {TUI_DETAIL_VERSIONS_FG};
    tui_render_wrapped(ver_sub, right, rw, lh - ry, ry, "Versions: ", gvs, vf);
  }
  free(gvs);
}

static void tui_search_result_show(cJSON *hits_array, int total_hits,
                                const char *query, const char *sort,
                                const char *loaders, char **game_versions, int game_versions_count,
                                const char *project_types, char **categories, int categories_count,
                                const char *author) {
  notcurses_options ncopts = {
    .loglevel = NCLOGLEVEL_SILENT,
    .flags = NCOPTION_SUPPRESS_BANNERS,
  };
  struct notcurses *nc = notcurses_core_init(&ncopts, NULL);
  if (!nc) return;

  unsigned dimy, dimx;
  notcurses_stddim_yx(nc, &dimy, &dimx);
  int hdr_h = 2, footer_h = 1, row_h = 2;
  struct tui_lay L = {
    .left_w = (int)dimx * 5 / 16,
    .right_w = (int)dimx - (int)dimx * 5 / 16,
    .list_h = (int)dimy - hdr_h - footer_h,
    .vis_rows = ((int)dimy - hdr_h - footer_h) / row_h,
  };
  struct ncplane *std = notcurses_stdplane(nc);
  L.hdr    = ncplane_create(std, &(struct ncplane_options){.y=0,.x=0,.rows=hdr_h,.cols=dimx});
  L.left   = ncplane_create(std, &(struct ncplane_options){.y=hdr_h,.x=0,.rows=L.list_h,.cols=L.left_w});
  L.right  = ncplane_create(std, &(struct ncplane_options){.y=hdr_h,.x=L.left_w,.rows=L.list_h,.cols=L.right_w});
  L.footer = ncplane_create(std, &(struct ncplane_options){.y=dimy-footer_h,.x=0,.rows=footer_h,.cols=dimx});
  ncplane_set_fg_rgb8(L.footer, TUI_FOOTER_FG);
  ncplane_printf_yx(L.footer, 0, 2, "j/k or Up/Down: navigate | q: quit");
  L.ver_sub = ncplane_create(L.right, &(struct ncplane_options){.y=0,.x=12,.rows=4,.cols=L.right_w-12,.name="versions"});
  L.sum_sub = ncplane_create(L.right, &(struct ncplane_options){.y=0,.x=12,.rows=4,.cols=L.right_w-12,.name="summary"});

  int sel = 0, sco = 0;
  do {
    tui_resize(nc, &L, hdr_h, footer_h, row_h);
    tui_render_hdr(L.hdr, total_hits, query, sort, loaders,
                   game_versions, game_versions_count, project_types,
                   categories, categories_count, author);
    tui_render_list(L.left, hits_array, total_hits, L.left_w, row_h, L.vis_rows, &sel, &sco);
    tui_render_detail(L.right, L.ver_sub, L.sum_sub, hits_array, sel, L.right_w, L.list_h);
    notcurses_render(nc);

    ncinput ni;
    uint32_t key = notcurses_get_nblock(nc, &ni);
    if (key == 0) {
      struct timespec ts = {0, 50000000};
      nanosleep(&ts, NULL);
      continue;
    }
    if (key == (uint32_t)-1) break;
    switch (ni.id) {
    case 'k': case NCKEY_UP:    sel--; break;
    case 'j': case NCKEY_DOWN:  sel++; break;
    case 'q': goto done;
    }
  } while (1);

done:
  ncplane_destroy(L.ver_sub);
  ncplane_destroy(L.sum_sub);
  notcurses_stop(nc);
}

void deal_new_filter(void) {
  char *tmp = NULL;
  char *p;
  char *sep;
  if (project_types && *project_types) {
    p = tmp; sep = tmp ? " AND " : "";
    m_asprintf(&tmp, "%s%sproject_types=[\"%s\"] ", tmp ? tmp : "", sep, project_types);
    free(p);
  }
  if (loaders && *loaders) {
    p = tmp; sep = tmp ? " AND " : "";
    m_asprintf(&tmp, "%s%scategories IN [\"%s\"] ", tmp ? tmp : "", sep, loaders);
    free(p);
  }
  if (game_versions_count > 0) {
    p = tmp; sep = tmp ? " AND " : "";
    char *list = m_strdup(game_versions[0]);
    for (int i = 1; i < game_versions_count; i++) {
      char *old = list;
      m_asprintf(&list, "%s\", \"%s", old, game_versions[i]);
      free(old);
    }
    m_asprintf(&tmp, "%s%sgame_versions IN [\"%s\"] ", tmp ? tmp : "", sep, list);
    free(list);
    free(p);
  }
  if (categories_count > 0) {
    p = tmp; sep = tmp ? " AND " : "";
    char *list = m_strdup(categories[0]);
    for (int i = 1; i < categories_count; i++) {
      char *old = list;
      m_asprintf(&list, "%s\", \"%s", old, categories[i]);
      free(old);
    }
    m_asprintf(&tmp, "%s%scategories IN [\"%s\"] ", tmp ? tmp : "", sep, list);
    free(list);
    free(p);
  }
  if (author && *author) {
    p = tmp; sep = tmp ? " AND " : "";
    m_asprintf(&tmp, "%s%sauthor IN [\"%s\"] ", tmp ? tmp : "", sep, author);
    free(p);
  }
  free(new_filters);
  if (tmp) {
    char *e = curl_encode(tmp);
    m_asprintf(&new_filters, "&new_filters=%s", e);
    free(e);
    free(tmp);
  } else {
    new_filters = m_strdup("");
  }
}

void init(void) {
  query = m_strdup("");
  new_filters = m_strdup("");
  categories = NULL;
  categories_count = 0;
  project_types = m_strdup("");
  game_versions = NULL;
  game_versions_count = 0;
  loaders = m_strdup("");
  sort = m_strdup("relevance");
  author = m_strdup("");
}

void cleanup(void) {
  free(query);
  free(new_filters);
  for (int i = 0; i < categories_count; i++) free(categories[i]);
  free(categories);
  free(project_types);
  for (int i = 0; i < game_versions_count; i++) free(game_versions[i]);
  free(game_versions);
  free(loaders);
  free(sort);
  free(author);
}
