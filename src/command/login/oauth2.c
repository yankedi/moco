//
// Created by root on 4/4/26.
//

#include "oauth2.h"
#include "command/init.h"
#include "curl.h"
#include "m_exit.h"
#include "utility/mtool.h"
#include "tomlc17.h"

#include <cjson/cJSON.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int deviceCode();
static int accessToken(const char *refresh_token_pre);
static int XBL();
static int XSTS();
static int minecraftToken();

static char *device_code = NULL;
static int interval = 5;
static char *access_token = NULL;
static char *refresh_token = NULL;
static char *XBL_token = NULL;
static char *uhs = NULL;
static char *XSTS_token = NULL;
static char *minecraft_token = NULL;
static char *username = NULL;

void oauth2Login(void) {
  printf("Waiting for device code...\n");
  if (deviceCode() != 0) {
    fprintf(stderr, "Failed to get device code\n");
    exit(-1);
  }
  printf("Waiting for authorization...\n");
  while (1) {
    int res = accessToken("first login");
    if (res == 0) {
      printf("Login successful!\n");
      break;
    } else if (res == 1) {
      sleep(interval);
    } else {
      fprintf(stderr, "Authorization failed or expired.\n");
      exit(-1);
    }
  }
  // printf("accessToken:%s\n", access_token);
  // printf("refresh_token:%s\n", refresh_token);
  free(device_code);
  if (XBL() != 0) {
    fprintf(stderr, "Failed to get XBL token\n");
    exit(-1);
  }
  free(access_token);
  // printf("XBL_token:%s\n", XBL_token);
  // printf("uhs:%s\n", uhs);
  if (XSTS() != 0) {
    fprintf(stderr, "Failed to get XSTS token\n");
    exit(-1);
  }
  // printf("XSTS_token:%s\n", XSTS_token);
  free(XBL_token);
  if (minecraftToken() != 0) {
    fprintf(stderr, "Failed to get Minecraft token\n");
    exit(-1);
  }
  // printf("username:%s\n",username);
  // printf("minecraftToken:%s\n",minecraft_token);
  free(XSTS_token);

  FILE *fp;
  if ((fp = fopen(".moco/account.toml", "r")) != NULL) {
    printf("Account already exists.\n");
    char *line = NULL;
    size_t len = 0;
    printf("Do you want to change your account? [y/N]: ");
    getline(&line, &len, stdin);
    if (rpmatch(line) == 1) {
      cmd("rm .moco/account.toml", "rm");
    } else {
      fclose(fp);
      exit(0);
    }
    free(line);
  }
  fp = fopen(".moco/account.toml", "w+");
  fprintf(fp, "[account]\n");
  fprintf(fp, "username = \"%s\"\n", username);
  fprintf(fp, "refresh_token = \"%s\"\n", refresh_token);
  fprintf(fp, "minecraft_token = \"%s\"\n", minecraft_token);
  fprintf(fp, "uhs = \"%s\"\n", uhs);
  fclose(fp);
  free(username);
  free(uhs);
  free(refresh_token);
  free(minecraft_token);
}
void oauth2Refresh(void) {
  if (access(".moco/account.toml", F_OK) == 0) {
    toml_result_t account_r = toml_parse_file_ex(".moco/account.toml");
    if (!account_r.ok) {
        fprintf(stderr, "Failed to parse account.toml: %s\n", account_r.errmsg);
        exit(-1);
    }

    toml_datum_t account_root = account_r.toptab;
    toml_datum_t refresh_token_res = toml_seek(account_root, "account.refresh_token");

    if (refresh_token_res.type == TOML_STRING) {
      char *old_refresh_token = m_strdup(refresh_token_res.u.s);
      toml_free(account_r);

      if (accessToken(old_refresh_token) == 0) {
        free(old_refresh_token);
        
        if (XBL() != 0) {
          fprintf(stderr, "Failed to get XBL token\n");
          exit(-1);
        }
        free(access_token);

        if (XSTS() != 0) {
          fprintf(stderr, "Failed to get XSTS token\n");
          exit(-1);
        }
        free(XBL_token);

        if (minecraftToken() != 0) {
          fprintf(stderr, "Failed to get Minecraft token\n");
          exit(-1);
        }
        free(XSTS_token);

        // 重新写入配置文件
        FILE *fp = fopen(".moco/account.toml", "w");
        if (fp != NULL) {
            fprintf(fp, "[account]\n");
            fprintf(fp, "username = \"%s\"\n", username);
            fprintf(fp, "refresh_token = \"%s\"\n", refresh_token);
            fprintf(fp, "minecraft_token = \"%s\"\n", minecraft_token);
            fprintf(fp, "uhs = \"%s\"\n", uhs);
            fclose(fp);
        }

        free(username);
        free(uhs);
        free(refresh_token);
        free(minecraft_token);

      } else {
        free(old_refresh_token);
        fprintf(stderr, "Session expired or network error. Please run 'moco login' again.\n");
        remove(".moco/account.toml");
        exit(-1);
      }
    } else {
        toml_free(account_r);
        fprintf(stderr, "No refresh token found. Please run 'moco login' first.\n");
        exit(-1);
    }
  } else {
    fprintf(stderr, "No account found. Please run 'moco login' first.\n");
    exit(-1);
  }
}
static int deviceCode() {
  const char *post_fields = "client_id=7ad33f37-472e-431d-af07-c1cf8cb0ff88&"
                            "scope=XboxLive.signin offline_access";
  char *response_body = NULL;

  if (curl_post("https://login.microsoftonline.com/consumers/oauth2/v2.0/devicecode",
          post_fields, &response_body) != 0) {
    return -1;
  }

  cJSON *response = cJSON_Parse(response_body);
  free(response_body);
  if (response == NULL) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
      fprintf(stderr, "JSON parse error at: %s\n", error_ptr);
    }
    cJSON_Delete(response);
    return -1;
  }

  cJSON *user_code_res = cJSON_GetObjectItemCaseSensitive(response, "user_code");
  cJSON *device_code_res = cJSON_GetObjectItemCaseSensitive(response, "device_code");
  cJSON *interval_res = cJSON_GetObjectItemCaseSensitive(response, "interval");

  if (!cJSON_IsString(user_code_res) || !cJSON_IsString(device_code_res)) {
    char *raw_error = cJSON_Print(response);
    if (raw_error != NULL) {
      printf("Verification failed. Original server response:\n%s\n", raw_error);
      free(raw_error);
    } else {
      fprintf(stderr,
              "Verification failed, but the response could not be printed.\n");
    }
    cJSON_Delete(response);
    return -1;
  }

  if (user_code_res->valuestring == NULL ||
      device_code_res->valuestring == NULL) {
    char *raw_error = cJSON_Print(response);
    if (raw_error != NULL) {
      printf("Verification failed. Original server response:\n%s\n", raw_error);
      free(raw_error);
    } else {
      fprintf(stderr,
              "Verification failed, but the response could not be printed.\n");
    }
    cJSON_Delete(response);
    return -1;
  }

  device_code = m_strdup(device_code_res->valuestring);
  if (device_code == NULL) {
    cJSON_Delete(response);
    return -1;
  }
  interval = interval_res->valueint;
  printf("Please visit https://www.microsoft.com/link and enter the code: %s\n",
         user_code_res->valuestring);

  cJSON_Delete(response);
  return 0;
}

static int accessToken(const char *refresh_token_pre) {
  char *post_fields = NULL;
  char *response_body = NULL;
  if (strcmp(refresh_token_pre, "first login") == 0)
  m_asprintf(
      &post_fields,
      "grant_type=urn:ietf:params:oauth:grant-type:device_code&client_id="
      "7ad33f37-472e-431d-af07-c1cf8cb0ff88&device_code=%s",
      device_code);
  else m_asprintf(&post_fields,
             "client_id=7ad33f37-472e-431d-af07-c1cf8cb0ff88"
             "&grant_type=refresh_token"
             "&refresh_token=%s"
             "&scope=XboxLive.signin%%20offline_access",
             refresh_token_pre);

  if (curl_post("https://login.microsoftonline.com/consumers/oauth2/v2.0/token",
                post_fields, &response_body) != 0) {
    free(post_fields);
    return -1;
  }
  free(post_fields);

  cJSON *response = cJSON_Parse(response_body);
  free(response_body);
  if (response == NULL) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
      fprintf(stderr, "JSON parse error at: %s\n", error_ptr);
    }
    cJSON_Delete(response);
    return -1;
  }

  cJSON *error_res = cJSON_GetObjectItemCaseSensitive(response, "error");
  if (cJSON_IsString(error_res) && error_res->valuestring != NULL) {
    if (strcmp(error_res->valuestring, "authorization_pending") == 0) {
      cJSON_Delete(response);
      return 1; // 返回 1 表示还在等，不报错
    }
    cJSON_Delete(response);
    return -1;
  }

  cJSON *access_token_res = cJSON_GetObjectItemCaseSensitive(response, "access_token");
  cJSON *refresh_token_res = cJSON_GetObjectItemCaseSensitive(response, "refresh_token");

  if (!cJSON_IsString(access_token_res) || !cJSON_IsString(refresh_token_res)) {
    char *raw_error = cJSON_Print(response);
    if (raw_error != NULL) {
      printf("Verification failed. Original server response:\n%s\n", raw_error);
      free(raw_error);
    } else {
      fprintf(stderr,
              "Verification failed, but the response could not be printed.\n");
    }
    cJSON_Delete(response);
    return -1;
  }

  if (access_token_res->valuestring == NULL ||
      refresh_token_res->valuestring == NULL) {
    char *raw_error = cJSON_Print(response);
    if (raw_error != NULL) {
      printf("Verification failed. Original server response:\n%s\n", raw_error);
      free(raw_error);
    } else {
      fprintf(stderr,
              "Verification failed, but the response could not be printed.\n");
    }
    cJSON_Delete(response);
    return -1;
  }

  access_token = m_strdup(access_token_res->valuestring);
  refresh_token = m_strdup(refresh_token_res->valuestring);

  cJSON_Delete(response);
  return 0;
}

static int XBL() {
  char *post_fields = NULL;
  char *response_body = NULL;
  m_asprintf(&post_fields,
             "{"
             "\"Properties\": {"
             "\"AuthMethod\": \"RPS\","
             "\"SiteName\": \"user.auth.xboxlive.com\","
             "\"RpsTicket\": \"d=%s\""
             "},"
             "\"RelyingParty\": \"http://auth.xboxlive.com\","
             "\"TokenType\": \"JWT\""
             "}",
             access_token);
  if (curl_post("https://user.auth.xboxlive.com/user/authenticate", post_fields,
                &response_body) != 0) {
    free(post_fields);
    return -1;
  }
  free(post_fields);
  cJSON *response = cJSON_Parse(response_body);
  free(response_body);
  if (response == NULL) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
      fprintf(stderr, "JSON parse error at: %s\n", error_ptr);
    }
    cJSON_Delete(response);
    return -1;
  }
  cJSON *XBL_token_res = cJSON_GetObjectItemCaseSensitive(response, "Token");
  cJSON *display_claims =
      cJSON_GetObjectItemCaseSensitive(response, "DisplayClaims");
  cJSON *xui = NULL;
  cJSON *first_xui = NULL;
  cJSON *uhs_res = NULL;
  if (cJSON_IsObject(display_claims)) {
    xui = cJSON_GetObjectItemCaseSensitive(display_claims, "xui");
  }
  if (cJSON_IsArray(xui)) {
    first_xui = cJSON_GetArrayItem(xui, 0);
  }
  if (cJSON_IsObject(first_xui)) {
    uhs_res = cJSON_GetObjectItemCaseSensitive(first_xui, "uhs");
  }
  if (!cJSON_IsString(uhs_res) || !cJSON_IsString(XBL_token_res)) {
    char *raw_error = cJSON_Print(response);
    if (raw_error != NULL) {
      printf("Verification failed. Original server response:\n%s\n", raw_error);
      free(raw_error);
    } else {
      fprintf(stderr,
              "Verification failed, but the response could not be printed.\n");
    }
    cJSON_Delete(response);
    return -1;
  }

  const char *uhs_value = uhs_res != NULL ? uhs_res->valuestring : NULL;
  const char *xbl_token_value =
      XBL_token_res != NULL ? XBL_token_res->valuestring : NULL;
  if (uhs_value == NULL || xbl_token_value == NULL) {
    char *raw_error = cJSON_Print(response);
    if (raw_error != NULL) {
      printf("Verification failed. Original server response:\n%s\n", raw_error);
      free(raw_error);
    } else {
      fprintf(stderr,
              "Verification failed, but the response could not be printed.\n");
    }
    cJSON_Delete(response);
    return -1;
  }
  uhs = m_strdup(uhs_value);
  XBL_token = m_strdup(xbl_token_value);
  cJSON_Delete(response);
  return 0;
}

static int XSTS() {
  char *post_fields = NULL;
  char *response_body = NULL;
  m_asprintf(&post_fields,
             "{"
             "\"Properties\": {"
             "\"SandboxId\": \"RETAIL\","
             "\"UserTokens\":[ \"%s\" ]"
             "},"
             "\"RelyingParty\": \"rp://api.minecraftservices.com/\","
             "\"TokenType\": \"JWT\""
             "}",
             XBL_token);
  if (curl_post("https://xsts.auth.xboxlive.com/xsts/authorize", post_fields,
                &response_body) != 0) {
    free(post_fields);
    return -1;
  }
  free(post_fields);
  cJSON *response = cJSON_Parse(response_body);
  free(response_body);
  if (response == NULL) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
      fprintf(stderr, "JSON parse error at: %s\n", error_ptr);
    }
    cJSON_Delete(response);
    return -1;
  }

  cJSON *XSTS_Token_res = cJSON_GetObjectItemCaseSensitive(response, "Token");
  if (!cJSON_IsString(XSTS_Token_res)) {
    char *raw_error = cJSON_Print(response);
    if (raw_error != NULL) {
      printf("Verification failed. Original server response:\n%s\n", raw_error);
      free(raw_error);
    } else {
      fprintf(stderr,
              "Verification failed, but the response could not be printed.\n");
    }
    cJSON_Delete(response);
    return -1;
  }

  if (XSTS_Token_res->valuestring == NULL) {
    char *raw_error = cJSON_Print(response);
    if (raw_error != NULL) {
      printf("Verification failed. Original server response:\n%s\n", raw_error);
      free(raw_error);
    } else {
      fprintf(stderr,
              "Verification failed, but the response could not be printed.\n");
    }
    cJSON_Delete(response);
    return -1;
  }
  XSTS_token = m_strdup(XSTS_Token_res->valuestring);
  cJSON_Delete(response);
  return 0;
}

static int minecraftToken() {
  char *post_fields = NULL;
  char *response_body = NULL;
  m_asprintf(&post_fields,
             ""
             "{"
             "\"identityToken\": \"XBL3.0 x=%s;%s\""
             "}",
             uhs, XSTS_token);
  if (curl_post(
          "https://api.minecraftservices.com/authentication/login_with_xbox",
          post_fields, &response_body) != 0) {
    free(post_fields);
    return -1;
  }
  free(post_fields);
  cJSON *response = cJSON_Parse(response_body);
  free(response_body);
  if (response == NULL) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
      fprintf(stderr, "JSON parse error at: %s\n", error_ptr);
    }
    cJSON_Delete(response);
    return -1;
  }
  cJSON *username_res = cJSON_GetObjectItemCaseSensitive(response, "username");
  cJSON *minecraft_token_res =
      cJSON_GetObjectItemCaseSensitive(response, "access_token");
  if (!cJSON_IsString(username_res) || !cJSON_IsString(minecraft_token_res)) {
    char *raw_error = cJSON_Print(response);
    if (raw_error != NULL) {
      printf("Verification failed. Original server response:\n%s\n", raw_error);
      free(raw_error);
    } else {
      fprintf(stderr,
              "Verification failed, but the response could not be printed.\n");
    }
    cJSON_Delete(response);
    return -1;
  }

  if (username_res->valuestring == NULL ||
      minecraft_token_res->valuestring == NULL) {
    char *raw_error = cJSON_Print(response);
    if (raw_error != NULL) {
      printf("Verification failed. Original server response:\n%s\n", raw_error);
      free(raw_error);
    } else {
      fprintf(stderr,
              "Verification failed, but the response could not be printed.\n");
    }
    cJSON_Delete(response);
    return -1;
  }
  username = m_strdup(username_res->valuestring);
  minecraft_token = m_strdup(minecraft_token_res->valuestring);
  cJSON_Delete(response);
  return 0;
}