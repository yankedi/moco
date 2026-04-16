#include "curl.h"
#include "utility/mtool.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t curl_callback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  struct MemoryBlock *memblock = (struct MemoryBlock *)userp;
  char *ptr = realloc(memblock->memory, memblock->size + realsize + 1);

  if (ptr == NULL) {
    fprintf(stderr, "OOM! (realloc returned NULL)\n");
    return 0;
  }

  memblock->memory = ptr;
  memcpy(&(memblock->memory[memblock->size]), contents, realsize);
  memblock->size += realsize;
  memblock->memory[memblock->size] = 0;
  return realsize;
}

int curl_post(const char *url, const char *post_fields, char **response_body) {
  CURL *curl = NULL;
  CURLcode res;
  struct MemoryBlock chunk;

  if (response_body == NULL) {
    return -1;
  }

  *response_body = NULL;
  chunk.memory = m_malloc(1);
  if (chunk.memory == NULL) {
    return -1;
  }
  chunk.memory[0] = '\0';
  chunk.size = 0;
  curl = curl_easy_init();
  if (curl == NULL) {
    free(chunk.memory);
    return -1;
  }
  struct curl_slist *headers = NULL;
  if (strstr(url, "user.auth") || strstr(url, "xsts.auth") ||
      strstr(url, "minecraftservices")) {
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  }
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
  // curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

  res = curl_easy_perform(curl);
  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  if (res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));

    curl_easy_cleanup(curl);
    free(chunk.memory);
    return -1;
  } else if (http_code > 400) { // 在获取accessToken时用户为未授权时等于400
    fprintf(stderr, "[HTTP status: %ld]\n", http_code);
    curl_easy_cleanup(curl);
    free(chunk.memory);
    return -1;
  }
  if (headers) {
    curl_slist_free_all(headers);
  }
  curl_easy_cleanup(curl);
  *response_body = chunk.memory;
  return 0;
}
