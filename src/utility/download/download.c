//
// Created by root on 4/5/26.
//

#include "download.h"

#include "curl/curl.h"
#include "utility/mtool.h"
#include "utility/store/store.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define RETRY 5

void *worker(void *arg);

int DownloadOne(package *p) {
  char *path = get_store_path(p->sha1);
  int flag = 0;
  if (access(path, F_OK) == 0) {
    link_mkdir(path, p->path);
    free(path);
    return 0;
  }
  if (path != NULL && access(path, F_OK) != 0) {
    FILE *fp = NULL;
    printf("Downloading:%s\n", p->url);
    char *tmp = get_tmp(p->store);
    printf("path:%s\n", path);
    CURL *curl = curl_easy_init();
    for (int i = 0; i < RETRY; ++i) {
      fp = fopen(tmp, "wb+");
      curl_easy_reset(curl);
      curl_easy_setopt(curl, CURLOPT_URL, p->url);
      curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
      CURLcode res = curl_easy_perform(curl);
      rewind(fp);
      char *sha1_check = m_sha1(fp);
      if (res != CURLE_OK || strcmp(sha1_check, p->sha1) != 0) {
        fprintf(stderr, "Download failed: %s\n", curl_easy_strerror(res));
        fprintf(stderr, "remote_Sha1:%s\n", p->sha1);
        fprintf(stderr, "local_Sha1:%s\n", sha1_check);
        printf("Retrying... (%d/5)\n", i);
      }
      if (strcmp(sha1_check, p->sha1) == 0) {
        flag = 1;
        free(sha1_check);
        fclose(fp);
        break;
      }
      free(sha1_check);
      fclose(fp);
    }
    if (flag) {
      if (rename(tmp, path) != -1) {
        printf("\033[32mDownload successful\033[0m\n");
        link_mkdir(path, p->path);
      } else {
        perror("Rename failed");
      }
    }
    free(tmp);
    free(path);
    curl_easy_cleanup(curl);
    if (flag)
      return 0;
    return -1;
  }
  return -1;
}

int DownloadAll(package *list, const int total) {
  long num_cores = sysconf(_SC_NPROCESSORS_ONLN);
  if (num_cores == -1)
    num_cores = 5;
  pthread_t threads[num_cores * 2];
  const long oneSize = total / (num_cores * 2);
  long back = -1;
  for (int i = 0; i < num_cores * 2; ++i) {
    OnePackages *OnePack = m_malloc(sizeof(OnePackages));
    OnePack->pack = list;
    OnePack->front = (int)(back + 1);
    if ((back + oneSize + oneSize) > total)
      OnePack->back = total - 1;
    else
      OnePack->back = (int)(back + oneSize);
    back += oneSize;
    pthread_create(&threads[i], NULL, worker, OnePack);
  }
  int success = 0;
  void *result;
  for (int i = 0; i < num_cores * 2; ++i) {
    pthread_join(threads[i], &result);
    t_result *res = result;
    success += res->success;
    free(res);
  }
  return success;
}

void *worker(void *arg) {
  OnePackages *p = arg;
  t_result *result = calloc(1, sizeof(t_result));
  for (int i = p->front; i <= p->back; ++i) {
    if (DownloadOne(&p->pack[i]) == 0)
      ++result->success;
    else
      ++result->fail;
  }
  free(p);
  return result;
}