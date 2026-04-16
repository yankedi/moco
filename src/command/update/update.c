//
// Created by root on 4/4/26.
//

#include "update.h"

#include "env.h"
#include "utility/download/download.h"
#include "utility/download/m_epoll.h"
#include "utility/mtool.h"

#include <string.h>

// static void testSource(void);
// static void downloadInit(void);

/*
const Source SOURCES[] = {
    [SOURCE_OFFICIAL] = {.name = "Mojang Official",
                         .version_manifest = "https://piston-meta.mojang.com",
                         .assets = "https://resources.download.minecraft.net"},
    [SOURCE_BMCLAPI] = {.name = "BMCLAPI",
                        .version_manifest = "https://bmclapi2.bangbang93.com",
                        .assets = "https://bmclapi2.bangbang93.com/assets"}};

SourceType current_source = SOURCE_OFFICIAL;
*/
void update(void) {

  printf("HOME:%s\n", HOME);
  printf("According to the XDG specification, the downloaded file will be "
         "stored in %s\n",
         XDG_DATA_HOME);

  char *manifest_path = NULL;
  m_asprintf(&manifest_path, "%s/version_manifest_v2.json", XDG_DATA_HOME);
  char *url = m_strdup(
      "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json");
  printf("Download version_manifest_v2.json ing....\n");
  package *manifest = m_malloc(sizeof(package));
  manifest->url = url;
  manifest->path = manifest_path;
  manifest->sha1 = m_strdup("-1");
  manifest->store = m_strdup(manifest_path);
  submit_download_task(manifest);
  free_package(manifest);

  // testSource();
  // downloadInit();
}
/*
static void testSource(void) {
  CURL *curl;
  CURLcode res;
  double ms[2] = {99999.0, 99999.0};

  curl = curl_easy_init();
  if (!curl)
    return;

  for (int i = 0; i < 2; i++) {
    char url[256];
    snprintf(url, sizeof(url), "%s/mc/game/version_manifest_v2.json",
             SOURCES[i].version_manifest);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);
    //curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64)
moco-launcher"); res = curl_easy_perform(curl); if (res == CURLE_OK) { double
total_time_sec; res = curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME,
&total_time_sec); if (res == CURLE_OK) { ms[i] = total_time_sec * 1000.0;
        printf("[Ping] %s: %.2f ms\n", SOURCES[i].name, ms[i]);
      }
    } else {
      fprintf(stderr, "[Ping] Failed for %s: %s\n", SOURCES[i].name,
              curl_easy_strerror(res));
    }
  }

  current_source = (ms[0] < ms[1]) ? SOURCE_OFFICIAL : SOURCE_BMCLAPI;

  printf("=> Selected optimal source: %s\n", SOURCES[current_source].name);
  char *path = NULL;
  m_asprintf(&path,"%s/config.toml",XDG_CONFIG_HOME);
  free(path);
  curl_easy_cleanup(curl);

  printf("HOME:%s\n", HOME);
  printf("According to the XDG specification, the downloaded file will be "
         "stored in %s\n",XDG_DATA_HOME);

  char *manifest_path = NULL;
  m_asprintf(&manifest_path, "%s/version_manifest_v2.json", XDG_DATA_HOME);
  char *url = NULL;
  m_asprintf(&url,"%s/mc/game/version_manifest_v2.json",SOURCES[current_source].version_manifest);
  printf("Epoll:Download version_manifest_v2.json ing....\n");
  package *manifest = m_malloc(sizeof(package));
  manifest->url = url;
  manifest->path = manifest_path;
  manifest->sha1 = m_strdup("manifast");
  manifest->store = m_strdup(manifest_path);
  submit_download_task(manifest);
  free_package(manifest);

  char *config_path;
  m_asprintf(&config_path,"%s/config.toml",XDG_CONFIG_HOME);
  FILE *fp = fopen(config_path,"wb+");
  fprintf(fp,"source = \"%s\"\n",SOURCES[current_source].name);
  fclose(fp);
  free(config_path);
}
*/
/*
static void downloadInit(void) {
  printf("HOME:%s\n", HOME);
  printf("According to the XDG specification, the downloaded file will be "
         "stored in %s\n",XDG_DATA_HOME);
         */
/*
char *path = NULL;
char *url = NULL;
m_asprintf(&path, "%s/version_manifest_v2.json", XDG_DATA_HOME);
m_asprintf(&url,"%s/mc/game/version_manifest_v2.json",SOURCES[current_source].version_manifest);
DownloadOne(path, url);
free(path);
free(url);
*/
/*
  CURL *curl;
  FILE *fp;
  CURLcode res;

  char *path = NULL;
  m_asprintf(&path, "%s/version_manifest_v2.json", XDG_DATA_HOME);
  fp = fopen(path,"wb");
  free(path);
  curl = curl_easy_init();
  if (curl) {
    char *url = NULL;
    m_asprintf(&url,"%s/mc/game/version_manifest_v2.json",SOURCES[current_source].version_manifest);
    curl_easy_setopt(curl,CURLOPT_URL,url);
    free(url);
    curl_easy_setopt(curl,CURLOPT_WRITEDATA,fp);
    curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,1L);
    printf("Download version_manifest_v2.json ing....\n");
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      fprintf(stderr, "Download failed: %s\n", curl_easy_strerror(res));
    }else printf("Download successful\n");
  }
  curl_easy_cleanup(curl);
  fclose(fp);
}
*/