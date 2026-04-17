//
// Created by root on 4/4/26.
//

#include "login.h"

#include "cjson/cJSON.h"
#include "command/login/curl.h"
#include "command/login/oauth2.h"
#include "curl/curl.h"
#include "tomlc17.h"
#include "utility/mtool.h"

#include <stdlib.h>

static int getprofile(void);

void login(void) {
  oauth2Login();
  getprofile();
}

static int getprofile(void) {
  CURL *curl = NULL;
  CURLcode res;
  struct MemoryBlock chunk;

  chunk.memory = m_malloc(1);
  chunk.memory[0] = '\0';
  chunk.size = 0;

  curl = curl_easy_init();
  if (curl == NULL) {
    free(chunk.memory);
    return -1;
  }

  curl_easy_setopt(curl, CURLOPT_URL,"https://api.minecraftservices.com/minecraft/profile");
  struct curl_slist *headers = NULL;
  char *auth_header = NULL;

  toml_result_t result = toml_parse_file_ex(".moco/account.toml");
  if (!result.ok) {
    fprintf(stderr, "Account data is empty or file not found\n");
    free(chunk.memory);
    curl_easy_cleanup(curl);
    return -1;
  }

  toml_datum_t root = result.toptab;
  toml_datum_t token = toml_seek(root, "account.minecraft_token");

  if (token.type == TOML_STRING) {
    m_asprintf(&auth_header, "Authorization: Bearer %s", token.u.s);
    headers = curl_slist_append(headers, auth_header);
    free(auth_header);
  }
  printf("token:%s\n", token.u.s);
  toml_free(result);

  headers = curl_slist_append(headers, "Accept: application/json");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);

  res = curl_easy_perform(curl);

  // 获取 HTTP 状态码
  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

  curl_slist_free_all(headers);

  if (res != CURLE_OK) {
    fprintf(stderr, "网络请求失败: %s\n", curl_easy_strerror(res));
  } else if (http_code >= 400) {
    fprintf(stderr, "请求失败，HTTP 状态码: %ld\n", http_code);
    fprintf(stderr, "服务器返回详情: %s\n", chunk.memory);
  } else {
    cJSON *response = cJSON_Parse(chunk.memory);
    if (response == NULL) {
      fprintf(stderr, "JSON 解析错误\n");
      free(chunk.memory);
      curl_easy_cleanup(curl);
      return -1;
    }

    cJSON *id_res = cJSON_GetObjectItemCaseSensitive(response, "id");
    cJSON *name_rss = cJSON_GetObjectItemCaseSensitive(response, "name");

    if (id_res == NULL || name_rss == NULL) {
      fprintf(stderr, "未能从响应中找到玩家信息。\n");
    } else {
      FILE *fp = fopen(".moco/profile.toml", "w+");
      if (fp == NULL) {
        fprintf(stderr, "无法写入 .moco/profile.toml，请检查目录是否存在！\n");
      } else {
        fprintf(fp, "[profile]\n");
        fprintf(fp, "id = \"%s\"\n", id_res->valuestring);
        fprintf(fp, "name = \"%s\"\n", name_rss->valuestring);
        fclose(fp);
        printf("\n Hello! %s\n", name_rss->valuestring);
      }
    }
    cJSON_Delete(response);
  }

  free(chunk.memory);
  curl_easy_cleanup(curl);
  return 0;
}